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

The early local WiFi API is intended for controlled development networks and does not yet represent the final authentication model. Do not expose the device directly to the public internet or an untrusted network.

Never commit real WiFi passwords, printer API keys, private certificates, access tokens, or activation material. Generated firmware artifacts must be published only through the controlled release workflow.
