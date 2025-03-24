from ledger_bitcoin import Chain, TransportClient, WalletPolicy
from ledger_bitcoin.client import NewClient as AppClient
from ledger_bitcoin.psbt import PSBT


CLA_APP = 0xE1
INS_CUSTOM_XOR = 128

if __name__ == '__main__':
    transport = TransportClient()
    client = AppClient(transport, chain=Chain.TEST)

    fpr = client.get_master_fingerprint()
    print(f"Fingerprint: {fpr.hex()}")

    if fpr.hex() != "f5acc2fd":
        print("This test assumes that the device is onboarded with the default mnemonic of Speculos")
        client.stop()
        exit(1)

    wallet = WalletPolicy(
        "",
        "wpkh(@0/**)",
        [
            "[f5acc2fd/84'/1'/0']tpubDCtKfsNyRhULjZ9XMS4VKKtVcPdVDi8MKUbcSD9MJDyjRu1A2ND5MiipozyyspBT9bg8upEp7a8EAgFxNxXn1d7QkdbL52Ty5jiSLcxPt1P"
        ],
    )
    psbt = PSBT()
    psbt.deserialize("cHNidP8BAgQCAAAAAQMEAAAAAAEEAQEBBQEBAfsEAgAAAAABAP0nAQEAAAAAAQH61dC3qzG4+0elZljwSfZzw4BO0k3YorFHqhmT5u8u9QAAAAAA/f///wIAo+ERAAAAACIAINriKfksl70DmN6D97sY1Uf/4HSlNLDKqxJtxnc7iQ00AAAAAAAAAABTakxQU0FUKwEEW95gt9Dmt1jKXdjGHTd6LF8a9R7BqeIJ9eoANsjC9BB4o86+5X2KR9UBBB9eDmaxdXapFBNH6CoDe127OM+MR1nyQrH1x+CaiKwCRzBEAiB+jg0MLSnxXcDbof13W8IFHTpm5/+wiTfvPny1T1ZS5AIgXgtvhtl4s8wC2pcTVr9MPQfi1dyF6x5b8aQYuKal3Q8BIQJ8t100sAXE659iu/LEV9djjoE+dX787I+mhnfZULY2YgAAAAABASsAo+ERAAAAACIAINriKfksl70DmN6D97sY1Uf/4HSlNLDKqxJtxnc7iQ00AQQgBB9eDmaxdXapFBNH6CoDe127OM+MR1nyQrH1x+CaiKwBBSAEH14OZrF1dqkUE0foKgN7Xbs4z4xHWfJCsfXH4JqIrCIGAny3XTSwBcTrn2K78sRX12OOgT51fvzsj6aGd9lQtjZiGPWswv1UAACAAQAAgAAAAIAAAAAAAAAAAAEOIC2nPuT61F9PDRV4f9qwMR0OD2gvPJbZo7MelOVNf+WVAQ8EAAAAAAEQBP3///8AIgICcbW3ea2HCDhYd5e89vDHrsWr52pwnXJPSNLibPh08KAY9azC/VQAAIABAACAAAAAgAEAAAAAAAAAAQMIAKPhEQAAAAABBBYAFDXG4N1tPISxa6iF3Kc6yGPQtZPsAA==")
    
    print(client.get_wallet_address(wallet, None, 0, 0, False))
    try:
        sign_results = client.sign_psbt(psbt, wallet, None)
    except Exception as e:
        print("Error signing PSBT:", e)
        client.stop()
        exit(1)

    print("Results of sign_psbt:", sign_results)

    assert len(sign_results) == 1

    signatures = list(sorted(sign_results))

    # Test that the signature is for the correct pubkey
    i_0, psig_0 = signatures[0]
    assert i_0 == 0
    # This is the key derived at m/84'/1'/0'/0/0
    print(psig_0.pubkey.hex())
    assert psig_0.pubkey == bytes.fromhex(
        "027cb75d34b005c4eb9f62bbf2c457d7638e813e757efcec8fa68677d950b63662")

    # Add partial signatures to the PSBT
    psbt.inputs[0].partial_sigs[psig_0.pubkey]  = psig_0.signature

    psbt.tx = psbt.get_unsigned_tx()
    psbt.version = 0
    print("Signed PSBT:", psbt.serialize())

    client.stop()
