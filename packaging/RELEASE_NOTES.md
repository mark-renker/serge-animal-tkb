**v2.0.1** is a packaging fix: every download now includes the AGPLv3 `LICENSE` and the `NOTICE`
(the installers also show a license page), and the v1 bundles zip no longer contains macOS `._` metadata
files that could break the plug-in's signature when extracted with non-Apple unzip tools.
The plug-in code is identical to v2.0.0.

**Downloads: no compiling needed.** See the full guide in
[packaging/INSTALL.md](https://github.com/mark-renker/serge-animal-tkb/blob/main/packaging/INSTALL.md).

- **macOS (Apple Silicon + Intel, macOS 11+):** `SergeAnimalTKB-v2-…-macOS.pkg`. It installs
  the AU, VST3 and standalone app. It isn't notarized: on first open use
  *System Settings → Privacy & Security → Open Anyway* (macOS 15+) or right-click → *Open* (macOS 11–14).
- **Original v1 (macOS):** `SergeAnimalTKB-v1-…-macOS.pkg`. It installs alongside v2.
- **Windows 10/11 (x64):** `SergeAnimalTKB-v2-…-Windows-x64.zip`. Copy the `.vst3` folder to
  `C:\Program Files\Common Files\VST3\`.
