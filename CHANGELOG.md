# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.1] - 2026-03-23

### Added

- Apex P poring (Ledger Nano Gen 5)
- Derivation Path Hardening:
    - New master key fingerprint syscall use, `HAVE_APPLICATION_FLAG_DERIVE_MASTER` is removed
    - BIP-32 derivation paths is reinforced using wildcard syntax (`m/*/<COIN_TYPE>`)

## [0.1.0] - 2025-03-13

### Added

- Initial boilerplate for derived applications with custom signing behaviors.

### Changed

- ...

### Fixed

- ...
