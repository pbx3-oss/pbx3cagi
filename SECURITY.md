# Security Policy

## Supported versions

Security fixes land on the default branch (`main`) of repositories under [pbx3-oss](https://github.com/pbx3-oss). Older tags/releases may not receive backports unless noted in a release advisory.

## Reporting a vulnerability

**Please do not open a public GitHub issue for security reports.**

Prefer GitHub’s private vulnerability reporting:

1. Open this repository on GitHub.
2. **Security** → **Advisories** → **Report a vulnerability** (or **Report a vulnerability** on the Security tab).

If private reporting is unavailable, contact the [pbx3-oss](https://github.com/pbx3-oss) organization owners via GitHub (do not paste secrets or exploit detail in a public channel).

We will acknowledge reports as soon as practical and coordinate a fix and disclosure timeline with you.

## Scope

In scope: vulnerabilities in code in this repository that affect confidentiality, integrity, or availability of PBX3 deployments when used as documented.

Out of scope (examples): social engineering, denial-of-service via unbounded public internet load without a clear product defect, and issues only in third-party runtime neighbours (e.g. Asterisk, OpenSIPS) that are not shipped as source in this repo.
