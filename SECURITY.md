# Security Policy

## Supported Versions

Security fixes are applied to the latest published firmware and the active `main` development branch. Older development builds may not receive backported fixes.

## Reporting A Vulnerability

Do not publish credentials, private network information, exploit details, or a working attack in a public issue.

Use GitHub's private vulnerability reporting feature for this repository. Include:

- affected commit or release;
- hardware and network setup;
- steps to reproduce;
- realistic impact;
- logs with credentials removed;
- a suggested mitigation when available.

## Current Development Warning

The local WiFi API requires a random per-device bearer token obtained during physically confirmed BLE pairing. API requests are size-limited and wildcard CORS is disabled. This is a local control protocol: do not forward its port, expose the device directly to the public internet, or treat application pairing as a substitute for network isolation.

The browser panel deliberately uses a reboot-scoped session that can be requested by a browser on the device's own local hostname or IP address. This prevents unrelated web origins from calling the API through a normal browser, but it does not authenticate people or software already present on the same LAN. Use coroNET only on a trusted local network. HTTP traffic, including Android bearer-token requests, is not encrypted on that network.

Pairing starts only from **Settings > Companion connection > Pair phone**. Starting it revokes the previous relationship and rotates the token. Both the coroNET touchscreen and phone must confirm the same six-digit code before the new token is released; factory reset and a new pairing session can revoke stored access.

The six-digit comparison is an application-level physical-presence check, not Bluetooth Secure Connections bonding. BLE GATT traffic is not claimed to resist a nearby active radio attacker. Adding mandatory operating-system bonding is deferred because it would change the recovery and re-pairing workflow and must be tested as a separate compatibility feature.

Never commit real WiFi passwords, printer API keys, private certificates, access tokens, or activation material. Generated firmware artifacts must be published only through the controlled release workflow.
