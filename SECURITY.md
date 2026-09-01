# Security Policy

## Supported Versions

coroNET OS 2 is currently pre-release software. Security fixes are applied to the latest development branch until the first stable release line is published.

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

The local WiFi API requires a random per-device bearer token obtained during BLE pairing. API requests are size-limited and wildcard CORS is disabled. This is still a local control protocol: do not forward its port, expose the device directly to the public internet, or treat the current pre-release pairing flow as a substitute for network isolation.

The first-pairing window currently opens automatically until a companion confirms token storage. Physical confirmation, token revocation, and pairing reset controls must be completed in the touchscreen UI before a stable product release.

Never commit real WiFi passwords, printer API keys, private certificates, access tokens, or activation material. Generated firmware artifacts must be published only through the controlled release workflow.
