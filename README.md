# 77.MP3

**Vocal Draw and Auto-Tune in one VST3 plugin, by [WajFay](https://www.instagram.com/wajfay).**

Built with JUCE for Windows and FL Studio. Black, flat interface. One engine runs at a time.

| Engine | What it does |
| --- | --- |
| **Vocal Draw** (default) | A Newtone-style pitch editor. The plugin captures your vocal as note blobs while FL plays. You can drag notes, draw the pitch curve, snap notes to the scale, or let it **auto draw from a reference vocal on the side-chain**. |
| **Auto-Tune** | Real-time pitch correction. Every note is pulled to the nearest note of the selected key and scale, even when the singer is far out of tune. You can set retune speed, humanize, mix and output. |

To switch engines, click the other engine in the header, or click the power icon of the running engine to hand over to the other one.

---

## Using it in FL Studio

### Vocal Draw with a reference vocal (side-chain)
1. Put **77.MP3** on the mixer track of the vocal you want to tune.
2. Select the mixer track that holds the **reference vocal** (guide, demo or original take). Right-click the arrow at the bottom of the target track and choose **Sidechain to this track**.
3. Open 77.MP3, click the **gear (wrapper settings) > Processing** tab, and set the side-chain input to that track.
4. Make sure **AUTO DRAW** is on, and press play. The header shows *SIDE-CHAIN · RECEIVING*. As the song plays:
   * grey blobs show your vocal,
   * the blue line shows the reference melody, transposed to your octave automatically,
   * the white line shows the correction the plugin applies.
5. Edit any spot by hand afterwards. Your edits and the captured curves are saved with the FL project.

### Vocal Draw by hand
| Tool | Key | Action |
| --- | --- | --- |
| Select | `1` | Drag a note up or down in semitones (hold **Alt** for cents). Double-click a note to snap it. Drag an empty area to scroll. |
| Pencil | `2` | Draw the pitch curve freehand (hold **Shift** to lock to semitones). |
| Line | `3` | Draw a straight pitch line. |
| Erase | `4` | Remove corrections. |

Other controls:
* **Snap Notes** tunes the whole take to the key/scale. **Variation** sets how much natural vibrato is kept.
* **From Reference** redraws the whole take from the captured reference.
* Right-click opens the note menu. Undo is `Ctrl+Z`. `Delete` resets the selected note.
* Mouse wheel scrolls notes. `Shift`+wheel scrolls time, `Ctrl`+wheel zooms time, and `Alt`+wheel zooms notes.

### Auto-Tune
Pick **Key** and **Scale** in the header, or click the note cells to build a custom scale.
* **Retune Speed** `0 ms` gives the hard, robotic effect. `20–60 ms` sounds natural.
* **Humanize** keeps the singer's vibrato on top of the corrected note.

---

## Building

The plugin and the installer are built automatically by GitHub Actions on every push
(`.github/workflows/build.yml`).

* **Artifacts** (Actions tab > latest run):
  * `77MP3-Setup-<version>-x64.exe`, the installer with the 77.MP3 logo
  * `77MP3-VST3-<version>-x64.zip`, the raw `77mp3.vst3` bundle
* **Release:** push a tag (`git tag v1.0.0 && git push --tags`) and the files are attached to a GitHub Release.

Local build (Visual Studio 2022 + CMake ≥ 3.22):
```bat
cmake -B build -A x64
cmake --build build --config Release
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\77mp3.iss
```

The installer puts `77mp3.vst3` in `C:\Program Files\Common Files\VST3\`. That is the standard folder FL Studio scans.

### Antivirus / SmartScreen
The build already does what it can to avoid false positives:
* It has full version info and the publisher is set to WajFay.
* The binary uses the static runtime and is not packed or compressed with an executable packer.
* It uses a standard Inno Setup installer.

Windows SmartScreen still warns about any **unsigned** new download until it builds up reputation. The workflow can sign the plugin and the installer for you. Add two repository secrets:
* `WINDOWS_CERT_PFX`: your code-signing certificate (.pfx), base64 encoded
* `WINDOWS_CERT_PASSWORD`: its password

If any engine flags a build, you can submit the file as a false positive to that vendor, for example Microsoft at https://www.microsoft.com/wdsi/filesubmission.

---

## Project layout
```
Source/
  PluginProcessor.*      engine routing, side-chain, timeline capture, state
  PluginEditor.*         header, engine switch, Vocal Draw page
  dsp/PitchDetector.h    YIN pitch detection (decimated, real-time)
  dsp/PsolaShifter.h     TD-PSOLA pitch shifter (formant-preserving)
  dsp/PitchTimeline.h    captured / reference / target pitch curves
  dsp/Scale.h            keys, scales, snapping
  ui/                    look and feel, piano roll, auto-tune page
Resources/               logo, icon, Inter font (OFL)
installer/               Inno Setup script, wizard artwork, icon, license
```

Latency is about 45 ms at 48 kHz. The plugin reports it to the host, so FL Studio compensates automatically.

© 2026 WajFay. JUCE is used under its respective license. Inter typeface is used under the SIL OFL 1.1.
