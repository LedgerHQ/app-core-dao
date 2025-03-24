const fs = require('fs');
const path = require('path');
const bitcoin = require('bitcoinjs-lib');
const secp256k1 = require('tiny-secp256k1');
const bip32 = require('bip32').BIP32Factory(secp256k1);
const ECPair = require('ecpair').ECPairFactory(secp256k1);
const tools = require('uint8array-tools');

function assertFieldExists(object, field) {
  if (!object[field] && object[field] !== 0) {
    throw new Error(`Missing field "${field}" in object: ${JSON.stringify(object)}`);
  }
}

function readJsonFileSync(filePath) {
  try {
    const absolutePath = path.resolve(filePath); // Get absolute path
    const data = fs.readFileSync(absolutePath, 'utf8');
    return JSON.parse(data);
  } catch (err) {
    console.error('Error reading or parsing JSON file:', err);
    return null; // Or throw the error, depending on your needs
  }
}

function extractXpub(xpub) {
  const regex = /\[[a-f0-9]{8}\/[0-9'\/]+\]([a-zA-Z0-9]+)$/;
  const match = xpub.match(regex);

  if (match && match[1]) {
    return match[1];
  } else {
    throw new Error(`Could not extract xpub from "${xpub}"`);
  }
}

function extractRootPath(xpub) {
  const regex = /\[[a-f0-9]{8}\/([0-9'\/]+)\]([a-zA-Z0-9]+)$/;
  const match = xpub.match(regex);

  if (match && match[1]) {
    return match[1];
  } else {
    throw new Error(`Could not extract root derivation path from "${xpub}"`);
  }
}

function extractMasterFingerprint(xpub) {
  const regex = /\[([a-f0-9]{8})\/[0-9'\/]+\]([a-zA-Z0-9]+)$/;
  const match = xpub.match(regex);

  if (match && match[1]) {
    return match[1];
  } else {
    throw new Error(`Could not extract root derivation path from "${xpub}"`);
  }
}

function bufferToUint8Array(buffer) {
  return new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
}

function derive(xpub, path, network) {
  return  Buffer.copyBytesFrom(bip32.fromBase58(extractXpub(xpub), network).derivePath(path).publicKey);
}

function toKeyOriginInfo(path, wallet, network) {
  const masterFingerprint = Buffer.copyBytesFrom(fromHex(extractMasterFingerprint(wallet)));
  let rootPath = extractRootPath(wallet);
  if (!rootPath.endsWith('/') && path[0] !== '/') {
    rootPath += '/';
  }
  let pubkey = derive(wallet, path, network);
  path = rootPath + path;
  if (!path.startsWith('m/')) {
    path = 'm/' + path;
  }
  return {masterFingerprint: masterFingerprint, path: path, pubkey: pubkey}
}

// Not the best implementation for Wallet Policies
function pathToScript(xpub, policy, path, network) {
  if (policy.includes("wpkh(")) {
    return bitcoin.payments.p2wpkh({
      pubkey: derive(xpub, path, network),
      network: network
    }, { validate: false }).output;
  } else {
    throw new Error(`Unsupported policy "${policy}"`);
  }
}

function pathToArray(path) {
  return path
          .replace(/m\//i, '')
          .split('/')
          .map(level =>
            level.match(/['h]/i) ? parseInt(level) + 0x80000000 : Number(level)
  );
}

function toHex(uint8Array) {
  return Array.from(uint8Array)
    .map((byte) => byte.toString(16).padStart(2, '0'))
    .join('');
}

function fromHex(hexString) {
  if (hexString.length % 2 !== 0) {
    throw new Error('Invalid hex string length');
  }

  const byteArray = new Uint8Array(hexString.length / 2);

  for (let i = 0; i < hexString.length; i += 2) {
    byteArray[i / 2] = parseInt(hexString.substr(i, 2), 16);
  }

  return byteArray;
}

function toBase64(bytes) {
  return tools.toBase64(bytes);
}

module.exports = {
  readJsonFileSync,
  pathToScript,
  assertFieldExists,
  derive,
  toHex,
  toKeyOriginInfo,
  fromHex,
  toBase64,
  pathToArray
};