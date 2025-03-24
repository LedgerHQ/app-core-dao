const bitcoin = require('bitcoinjs-lib');
const { forgePrevouts } = require('./forge_prevout.js');
const {
  readJsonFileSync,
  assertFieldExists,
  pathToScript,
  toKeyOriginInfo,
  toBase64,
  toHex,
  pathToArray,
  derive,
} = require('./utils.js');

const { PsbtV2 } = require('ledger-bitcoin');
const { fromHex } = require('uint8array-tools');
const { fromBase64 } = require('uint8array-tools');
const { printPSBT } = require('./print_psbt.js');


function confObjectToOutput(conf, wallet, policy, network) {
  let script;
  let path = null;

  if (conf.address) {
    script = Buffer.copyBytesFrom(bitcoin.address.toOutputScript(conf.address, network));
  } else if (conf.script) {
    script = Buffer.from(conf.script, 'hex');
  } else if (conf.path) {
    script = pathToScript(wallet, policy, conf.path, network);
    path = toKeyOriginInfo(conf.path, wallet, network);
  } else {
    throw new Error(`Missing 'address' or 'script' field in ${JSON.stringify(conf)}`);
  }
  if (conf.recipient) {
    path = toKeyOriginInfo(conf.recipient, wallet, network);
  }
  assertFieldExists(conf, 'value');
  return { script: script, value: conf.value, path: path };
}

function createPSBT(prevouts, conf, network) {
  const psbt = new PsbtV2();

  // Conf checks
  assertFieldExists(conf, 'tx');
  assertFieldExists(conf, 'wallet');
  assertFieldExists(conf, 'policy');
  assertFieldExists(conf.tx, 'version');
  assertFieldExists(conf.tx, 'inputs');
  assertFieldExists(conf.tx, 'outputs');

  // Top level
  psbt.setGlobalTxVersion(conf.tx.version);
  psbt.setGlobalPsbtVersion(2);
  psbt.setGlobalFallbackLocktime(conf.tx.locktime ? conf.tx.locktime : 0);
  psbt.setGlobalInputCount(conf.tx.inputs.length);
  psbt.setGlobalOutputCount(conf.tx.outputs.length);

  // Add inputs
  for (let i = 0; i < conf.tx.inputs.length; i++) {
    const input = conf.tx.inputs[i];
    const prevout = prevouts[i];
    const tx = bitcoin.Transaction.fromBuffer(prevout.tx, network);
    psbt.setInputSequence(i, input.sequence ? input.sequence : 0xfffffffd);
    if (input.sighash_type) {
      psbt.setInputSighashType(i, input.sighashType);
    }
    psbt.setInputPreviousTxId(i, Buffer.copyBytesFrom(fromHex(tx.getHash())));
    psbt.setInputOutputIndex(i, prevout.index);
    psbt.setInputNonWitnessUtxo(i, prevout.tx);

    // Add bip32 derivation
    let path = null;
    if (input.recipient) {
      path = input.recipient;
    } else if (input.forge && input.forge.recipient) {
      path = input.forge.recipient;
    }
    if (path) {
      const keyInfo = toKeyOriginInfo(path, conf.wallet, network);
      psbt.setInputBip32Derivation(i, keyInfo.pubkey, keyInfo.masterFingerprint, pathToArray(keyInfo.path));
    }
    // TODO support non-witness UTXO and over address types
    psbt.setInputWitnessUtxo(i, tx.outs[prevout.index].value, tx.outs[prevout.index].script);
    
    if (input.redeem_script) {
      psbt.setInputRedeemScript(i, fromHex(input.redeem_script));
      psbt.setInputWitnessScript(i, fromHex(input.redeem_script));
    }
  }

  // Add outputs
  for (let i = 0; i < conf.tx.outputs.length; i++) {
    const output = confObjectToOutput(conf.tx.outputs[i], conf.wallet, conf.policy, network);
    psbt.setOutputAmount(i, output.value);
    psbt.setOutputScript(i, output.script);
    if (output.path) {
      psbt.setOutputBip32Derivation(i, output.path.pubkey, output.path.masterFingerprint, pathToArray(output.path.path));
    }
  }
  return psbt.serialize();
}

if (require.main === module) {
  if (process.argv.length < 3) {
    console.error('Usage: node create_psbt.js <conf.json>');
    process.exit(1);
  }
  try {
    const conf = readJsonFileSync(process.argv[2]);
    forgePrevouts(conf, bitcoin.networks.testnet);
    const psbt = createPSBT(prevouts, conf, bitcoin.networks.testnet);
    const parsed = new PsbtV2();
    parsed.deserialize(psbt);
    printPSBT(parsed, bitcoin.networks.testnet);
    console.log(toBase64(psbt));
  } catch (error) {
    console.error('Error:', error);
  }
}

module.exports = {
  createPSBT,
};