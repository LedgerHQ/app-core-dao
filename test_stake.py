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
    psbt.deserialize("cHNidP8BAgQCAAAAAQMEAAAAAAEEAQEBBQECAfsEAgAAAAABAMACAAAAAAEBkteTU5STYpaazD6mm2dBYgUIh1J35DYGPfH2tMV/iEEAAAAAAP////8BgDl6EgAAAAAWABQTR+gqA3tduzjPjEdZ8kKx9cfgmgJIMEUCIQCJ2mCr7T1A+h807JBkjVqj1lbKUoEB7FVqyeQUkbiW4AIgC1q0vsCiDGu2zqgACafrg3XsPsWPIJk6VIeB9iedgEcBIQM90rAt3EwCSzePotxDq2uBMYtEizXhd7qP26TzCQZ8IAAAAAABAR+AOXoSAAAAABYAFBNH6CoDe127OM+MR1nyQrH1x+CaIgYCfLddNLAFxOufYrvyxFfXY46BPnV+/OyPpoZ32VC2NmIY9azC/VQAAIABAACAAAAAgAAAAAAAAAAAAQ4g+tXQt6sxuPtHpWZY8En2c8OATtJN2KKxR6oZk+bvLvUBDwQAAAAAARAE/f///wABAwgAo+ERAAAAAAEEIgAg2uIp+SyXvQOY3oP3uxjVR//gdKU0sMqrEm3GdzuJDTQAAQMIAAAAAAAAAAABBFNqTFBTQVQrAQRb3mC30Oa3WMpd2MYdN3osXxr1HsGp4gn16gA2yML0EHijzr7lfYpH1QEEH14OZrF1dqkUE0foKgN7Xbs4z4xHWfJCsfXH4JqIrAA=")
    
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
