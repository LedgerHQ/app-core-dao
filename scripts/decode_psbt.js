const bitcoin = require('bitcoinjs-lib');

const psbt = bitcoin.Psbt.fromBase64("cHNidP8BAgQCAAAAAQMEAAAAAAEEAQEBBQECAfsEAgAAAAABAL8CAAAAAAEBnKCzdyEYm5y7sj1HRjpXBDIFvZcpRi4yP1NYoipfQ8QAAAAAAP////8BgDl6EgAAAAAWABQTR+gqA3tduzjPjEdZ8kKx9cfgmgJHMEQCIGK5aTRZHP/KMRnp7UtcjhBAz59Erbj07hRyoEAyf403AiBHEmOAqu/Vv70oIAyurQZ9cTKzSGQAhIm8dkh1yIUtDgEhAqNf5MI9x+6uz6avfb0iRDjFD9+Rleg0Buta/Ueh/if6AAAAAAEBH4A5ehIAAAAAFgAUE0foKgN7Xbs4z4xHWfJCsfXH4JoiBgJ8t100sAXE659iu/LEV9djjoE+dX787I+mhnfZULY2Yhj1rML9VAAAgAEAAIAAAACAAAAAAAAAAAABDiAleFPG2PlMpMqUmdZ/eHqVRL1J+EOHM/88OBPCb8YhBQEPBAAAAAABEAT9////AAEDCACj4REAAAAAAQQZdqkU9C5GLLOMvs37Z9t/sc+r6AfJSDOIrAABAwgAAAAAAAAAAAEEBWoDRk9PAA==")
console.log(psbt);