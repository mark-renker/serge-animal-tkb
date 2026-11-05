# Installing Serge Animal TKB

No compiling needed. Download an installer from the
[Releases page](https://github.com/mark-renker/serge-animal-tkb/releases).

| File | What it is |
|---|---|
| `SergeAnimalTKB-v2-<version>-macOS.pkg` | **v2** for macOS 11+ (Apple Silicon and Intel): AU, VST3, standalone app |
| `SergeAnimalTKB-v1-<version>-macOS.pkg` | The original **v1** (Faust) plug-in: AU and VST3. It can be installed alongside v2 |
| `SergeAnimalTKB-v2-<version>-Windows-x64.zip` | **v2** for Windows 10/11: VST3 and standalone `.exe` |
| `…-macOS-bundles.zip` | The raw macOS plug-in bundles, if you'd rather copy them by hand |

## macOS

1. Download the `.pkg` and double-click it.
2. **If macOS says it "can't verify the developer"**, that's expected. The installer
   is not notarized with a paid Apple Developer ID.
   - **macOS 15 (Sequoia) and later:** click **Done**, open **System Settings →
     Privacy & Security**, scroll down to the message about the installer, click
     **Open Anyway**, and confirm with your password.
   - **macOS 11–14:** right-click (or Control-click) the `.pkg`, choose **Open**,
     then **Open** again.
3. Follow the installer. On the **Installation Type** screen you can pick
   which formats to install: Audio Unit, VST3, standalone app.
4. Restart your DAW, or rescan plug-ins. The synth is listed under **Renky Denk**
   as *Serge Animal TKB v2* (v1: *Serge Animal TKB*).

You only have to approve the installer once. Plug-ins installed by it load
normally, with no further security prompts.

**Where things go:** `/Library/Audio/Plug-Ins/Components` (AU),
`/Library/Audio/Plug-Ins/VST3` (VST3), `/Applications` (standalone app).

**Uninstall:** delete `Serge Animal TKB v2.component`, `Serge Animal TKB v2.vst3`
and `Serge Animal TKB v2.app` from those folders.

**Copying bundles by hand (from the `-bundles.zip`):** put the `.component` in
`~/Library/Audio/Plug-Ins/Components` and the `.vst3` in
`~/Library/Audio/Plug-Ins/VST3`. Files downloaded this way are quarantined by
macOS, so clear that once in Terminal:

    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Serge Animal TKB v2.component" \
                                   ~/Library/Audio/Plug-Ins/VST3/"Serge Animal TKB v2.vst3"

## Windows

1. Unzip `SergeAnimalTKB-v2-<version>-Windows-x64.zip`.
2. Copy the `Serge Animal TKB v2.vst3` folder to
   `C:\Program Files\Common Files\VST3\`.
3. Rescan plug-ins in your DAW.
4. The standalone `Serge Animal TKB v2.exe` runs from anywhere. If Windows
   SmartScreen warns about an unrecognized app, click **More info → Run anyway**.
   The app isn't code-signed.

## Using it

- **MIDI:** notes play the default voice (pitch → oscillator, gate → envelope,
  velocity → level).
- **Presets:** the *PRESET* menu (top bar), or your DAW's program list: *Default
  Voice* plus six patches recreated from the v1 demos.
- **Patching:** drag from any jack to another to add a cable; right-click a jack
  to remove its cables; *CLEAR* removes all cables.
- **Zoom:** drag the window's bottom-right corner, or use the *ZOOM* menu. Past
  screen size the panel scrolls.
- **Knobs:** drag up/down (Shift = fine). The scroll wheel scrolls the panel;
  hold Option (Alt) to turn a knob with the wheel.

## For maintainers: making a release

- **One command, locally (macOS):**
  `packaging/make_installer.sh v2 --build` → `dist/…-macOS.pkg` + bundles zip.
  `packaging/make_installer.sh v1` packages the installed universal v1 plug-ins.
- **In the cloud:** push a tag (`git tag v2.0.1 && git push origin v2.0.1`).
  `.github/workflows/release.yml` builds the universal macOS installer and the
  Windows VST3 zip, runs the DSP test suite, and publishes a GitHub Release. The
  v1 installer isn't built in the cloud (it needs the Faust toolchain), so
  attach it once by hand:
  `gh release upload <tag> dist/SergeAnimalTKB-v1-1.0.0-macOS.pkg`.
- **Notarized builds (optional, warning-free installs):** with an Apple
  Developer ID, set `DEVELOPER_ID_APP="Developer ID Application: …"`,
  `DEVELOPER_ID_INSTALLER="Developer ID Installer: …"` and
  `NOTARY_PROFILE=<notarytool keychain profile>` before running
  `make_installer.sh`. It then signs with the hardened runtime, notarizes and
  staples.
