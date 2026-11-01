# Security policy

## Supported versions

Only the latest release receives security fixes.

## Reporting a vulnerability

Please **do not** open a public issue. Use GitHub's private reporting:
<https://github.com/sobhanmohammadi-dev/Liq2048/security/advisories/new>

Include a description, affected version/platform and reproduction steps. You can expect an acknowledgement
within a few days and a fix or mitigation plan as soon as it is assessed.

## Scope notes

Liq2048 is an offline game: it makes no network connections and collects no data. The realistic attack
surface is local file parsing (the `save.txt` progress file and `assets/lang/*.json`, `assets/fonts`,
`assets/sounds`, `assets/music`) and the third-party libraries it links (SDL2, SDL2_ttf, SDL2_mixer).
