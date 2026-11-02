## Install

| Platform | File | How |
|----------|------|-----|
| Windows | `*-windows-*.zip` | Extract and run `liq2048.exe` |
| macOS | `*-Darwin-*.zip` | Extract and run `liq2048` (universal: Apple Silicon + Intel). First launch: right-click, Open |
| Linux | `*-Linux-*.tar.gz` | Extract and run `./liq2048` (SDL is linked statically) |
| Android | `Liq2048-*-android.apk` | Allow "install unknown apps" and open the APK. The `.aab` is for Google Play |
| iOS | `Liq2048-*-ios-unsigned.ipa` | Unsigned: sideload with AltStore, Sideloadly or your own Apple developer certificate |

Keep the `assets/` folder next to the desktop executable.
The Android APK is signed with a debug key unless the repository has signing secrets configured, so updating
between builds from different keys requires uninstalling first.

Liq2048 is free and open source: no ads, no purchases, no tracking, no network access.
See [CHANGELOG.md](CHANGELOG.md) for details.
