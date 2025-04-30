# PSBT Forger
Small utility tool to create PSBT. The can generate fake UTXO used to test PSBT signature with Ledger device.
This tool is quite limited and only supports WPKH wallet (as the account managed by the device)

It generates base64 PSBT from a JSON file. You can find the JSON files used for CoreDAO in the `fixtures` folder.

Install
`yarn install`

Forge a PSBT
`node create_psbt.js <path to JSON file>`