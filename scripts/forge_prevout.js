const bitcoin = require('bitcoinjs-lib');
const bip32 = require('bip32');
const ecdsa = require('ecdsa');
const ECPairFactory = require('ecpair').ECPairFactory;
const secp256k1 = require('tiny-secp256k1');
const ECPair = ECPairFactory(secp256k1);
const Buffer = require('buffer').Buffer;
const crypto = require('crypto');
const { readJsonFileSync, pathToScript, assertFieldExists, toHex } = require('./utils.js');

function createFakeSignedTransaction(outputs, inputCount, amount, network) {
  if (!Array.isArray(outputs) || Number.isNaN(inputCount) || outputs.length === 0 || inputCount === 0) {
    throw new Error('Invalid inputs.');
  }

  const tx = new bitcoin.Psbt(network);
  // Fake inputs (replace with real UTXOs for real transactions)
  const fakeInputs = [];
  for (let i = 0; i < inputCount; i++) {
    const keyPair = ECPair.makeRandom({ network });
    const fakeTxid = generateFakeTxid();
    const fakeVout = 0;
    const fakeValue = Math.floor(amount / inputCount);
    const output = bitcoin.payments.p2wpkh({ pubkey: Buffer.copyBytesFrom(keyPair.publicKey), network });
    const fakeWitness = {
      script: output.output,
      value: fakeValue,
    };
    tx.addInput({hash: fakeTxid, index: fakeVout, sequence: 0xffffffff, witnessUtxo: fakeWitness }); // sequence number is set to max.
    fakeInputs.push({
      keyPair,
      value: fakeValue,
    });
  }

  // Add outputs
  outputs.forEach((output) => {
    tx.addOutput({script: output, value: amount});
  });

  // Sign inputs
  for (let i = 0; i < fakeInputs.length; i++) {
    const { keyPair, value } = fakeInputs[i];

    // Rewriting everything for the god damn type checker
    const signer = {publicKey: Buffer.copyBytesFrom(keyPair.publicKey)}
    signer.getPublicKey = function() { return this.publicKey; }
    signer.sign = function(hash, lowR) { 
      return Buffer.copyBytesFrom(keyPair.sign(hash, lowR));
    }

    tx.signInput(i, signer);
  }
  tx.finalizeAllInputs();
  return tx.extractTransaction(true).toHex();
}

function generateFakeTxid() {
  return crypto.randomBytes(32);
}

function forgePrevouts(conf, network) {
  // Iterate through inputs and forge transaction marked as forged
  prevouts = [];

  for (let i = 0; i < conf.tx.inputs.length; i++) {
    const input = conf.tx.inputs[i];
    if (input.forge) {
      assertFieldExists(input.forge, 'recipient');
      assertFieldExists(conf, 'wallet');
      assertFieldExists(conf, 'policy');
      assertFieldExists(input.forge, 'inputCount');
      assertFieldExists(input.forge, 'amount');
      
      outputScript =  pathToScript(conf.wallet, conf.policy, input.forge.recipient, network);
      const tx = createFakeSignedTransaction([outputScript], input.forge.inputCount, input.forge.amount, network); 
      prevouts.push({ tx: Buffer.from(tx, 'hex'), index: 0 });
    } else {
      prevouts.push({ tx: Buffer.from(input.tx, "hex"), index: input.index });
    }
  }
  return prevouts;
}

if (require.main === module) {
  if (process.argv.length < 3) {
    console.error('Usage: node forge_prevout.js <conf.json>');
    process.exit(1);
  }
  try {
    const conf = readJsonFileSync(process.argv[2]);
    prevouts = forgePrevouts(conf, bitcoin.networks.testnet);
    prevouts.forEach((prevout) => {
      console.log("Prevout:");
      console.log(toHex(prevout.tx));
      console.log("Output index:", prevout.index);
      console.log();
    });
  } catch (error) {
    console.error('Error:', error);
  }
}

module.exports = {
  createFakeSignedTransaction,
  forgePrevouts
};