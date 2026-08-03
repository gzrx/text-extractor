# text-extractor

Screen text extraction for KDE Plasma on Wayland. Press a key, drag a region,
and the text is in your clipboard — the Windows 11 Snipping Tool's text
extractor, done natively for KWin.

```
hotkey → capture workspace → drag a region → OCR → clipboard
```

**Status:** working end to end, with layout-aware assembly, a measured fixture
corpus, and a second OCR engine on demand. M0–M7a of 8 are done; what remains is
packaging. See [Roadmap](#roadmap).

## Why, given Spectacle already does this

Plasma 6.6 added an "Extract Text" button to Spectacle. That covers the simple
case well. This project exists for the part it doesn't do: treating OCR output
as *structured* rather than as one flat string.

Code, prose, tables, and non-English text each want contradictory handling. Code
must never be autocorrected and needs its indentation preserved; prose wants its
wrapped lines rejoined; tables need column reconstruction rather than words run
together. The region is classified and reassembled accordingly, with a heavier
engine available on a second key when the fast path is not good enough.

Every accuracy claim here is measured against a committed ten-fixture corpus
rather than asserted, and a change that moves no score does not earn its place.

## Requirements

Built and tested on Arch (CachyOS), Plasma 6.7.3, KWin 6.7.3, Wayland.

```bash
sudo pacman -S --needed cmake extra-cmake-modules qt6-base layer-shell-qt \
                        tesseract tesseract-data-eng leptonica \
                        hunspell hunspell-en_us onnxruntime \
                        kglobalaccel kguiaddons kconfig knotifications \
                        ki18n kwindowsystem
```

Add language data as needed — `tesseract-data-msa`, `tesseract-data-chi_sim`,
`tesseract-data-ara`, and so on. Configure which ones are used with
`textract --configure`.

`onnxruntime` is the CPU build and is a hard dependency, needed to link. The
CUDA stack was measured and rejected: it costs roughly 6 GiB against 25 MiB and
buys nothing at this model size. Tier 2's model files are a separate ~31 MB
runtime download — see [Two tiers](#two-tiers).

`hunspell` is a build dependency; the dictionaries are not. The dictionary is
consulted for one decision — whether a hyphen at a line end was typesetting or
part of the word — and quietly disables itself when it finds no
`en_US.{aff,dic}`, so a machine without `hunspell-en_us` still extracts text.

X11 is not supported. The capture path is KWin-specific and there is no
`xdg-desktop-portal` fallback yet.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Install

```bash
sudo cmake --install build
kbuildsycoca6
```

That `kbuildsycoca6` is **not optional** — see below.

### Running from the build tree

KWin authorises screen capture by matching the running executable against an
installed desktop entry, so an uninstalled binary needs its own:

```bash
cp build/org.kde.textract.dev.desktop ~/.local/share/applications/
kbuildsycoca6
```

CMake generates that file with an absolute path to your build output.

## Usage

```bash
textract --daemon          # resident; listens for the global shortcut
```

Then press the **Calculator key** (`XF86Calculator`), drag a region, release.
The recognised text lands on your clipboard, laid out according to what the
region turned out to be. Hold **Shift** through the drag to force raw output
when the classifier guesses wrong.

Rebind it in *System Settings → Shortcuts → textract*. The default avoids
modifier combinations because most are already taken by Plasma.

### Two tiers

**Shift+Calculator** re-runs the *same crop* — no re-capture, no second drag —
through PP-OCRv6_small on ONNX Runtime, and tells you whether the result
changed. Install the models first:

```bash
textract --fetch-models        # ~31 MB, SHA-256 verified, into ~/.local/share/textract/models
```

It is an escalation rather than a replacement, because neither engine wins
everywhere. Tier 2 is decisively better on CJK and on code, and Tesseract is
still better on small monospace terminal text. Tier 2 also needs no language
setting: it carries one fixed 18708-character set.

Run it at login with the provided unit:

```bash
cp data/textract.service ~/.config/systemd/user/
systemctl --user enable --now textract
```

Two diagnostic modes are also available:

```bash
textract --capture-test /tmp/shot.png   # capture only; verifies authorisation
textract --select-test  /tmp/crop.png   # capture + drag; writes the crop
```

## Configuration

```bash
textract --configure
```

Opens a dialog for the tier-1 OCR languages and the tier-2 model directory.
Settings live in `~/.config/textractrc` and a running daemon picks up changes
immediately — no restart.

Upscale factor and binarisation are readable from that file but are not in the
dialog. Their defaults are measured against the fixture corpus, and changing
them is an escape hatch for content the corpus does not cover rather than a
tuning knob.

Shortcuts are set in **System Settings → Shortcuts → textract**. The built-in
defaults are Calculator for tier 1 and Shift+Calculator for tier 2.

## How it works

The daemon captures the **entire workspace up front**, then lets you drag a
rectangle out of an image already in RAM. That means no latency between mouse-up
and OCR, one D-Bus round trip instead of two, and no need for a live transparent
overlay over the real desktop — which Wayland makes awkward.

```
src/
├── capture/     KWin ScreenShot2 D-Bus client
├── overlay/     LayerShellQt region selector
├── preprocess/  polarity, upscale, grayscale
├── ocr/         OcrEngine interface, Tesseract and PP-OCRv6 implementations
├── models/      tier-2 manifest, checksummed download, atomic install
├── analyze/     what kind of region is this
├── order/       reading order for engines that supply none
├── assemble/    words → text, one branch per kind
├── correct/     Hunspell dictionary, for the hyphen decision
├── config/      settings, and the dialog behind --configure
├── clipboard/   KSystemClipboard wrapper
└── app/         daemon, global shortcuts, notifications
```

Anything that can be a pure function is one. `capture/`, `overlay/`, the config
dialog and the daemon controller need a live compositor or live engines and are
verified by hand; the rest is pure data transformation with real unit tests,
which is what makes accuracy work iterable.

The whole pipeline between capture and clipboard lives behind one function, so
the daemon and the fixture harness run byte-for-byte the same path. A harness
that scored a different path would measure nothing.

The daemon is resident rather than fire-and-exit for two reasons: Tesseract's
`Init()` costs ~240ms and would otherwise be paid on every capture, and on
Wayland **clipboard contents die with the client that set them** — a one-shot
tool would copy text that vanishes the moment it exits.

## Gotchas worth knowing

Five things cost real debugging time here. Documented in case they save someone
else the same afternoon.

**KWin screenshot authorisation needs three things, not one.** Missing any one
gives `NoAuthorized`:

1. `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2` — case-sensitive.
   The lowercase `org.kde.kwin.screenshot` in older forum posts is the legacy v1
   interface and does not authorise v2.
2. `Exec=` must be an **absolute** path matching the running binary exactly.
3. The entry must be in **ksycoca** via `kbuildsycoca6`. KDE resolves services
   through ksycoca; `update-desktop-database` does not touch it.

KWin caches the grant per executable path, so a binary that once worked keeps
working after you delete its desktop file. Test negative cases with a copy at a
fresh path.

**`QScreen::devicePixelRatio()` lies under fractional scaling.** On a 1.25-scaled
display it reports `2` — the integer buffer scale. The true ratio is on
`QWindow::devicePixelRatio()`. Using the screen's value silently doubles every
coordinate.

**`asyncCall()` doesn't send anything without a running event loop**, and
`QDBusUnixFileDescriptor` *duplicates* the fd you hand it, so your own process
keeps a write end of the pipe open and `read()` never sees EOF. Draining a
capture pipe by waiting for EOF deadlocks twice over. Track the expected byte
count (`stride × height`) instead.

**`LayerShellQt::Shell::useLayerShell()` must run before any window exists.** It
swaps the Wayland shell integration; windows created earlier get an ordinary
surface type.

**`KConfigWatcher` only fires for writes flagged `KConfigBase::Notify`.** A
plain `writeEntry()` followed by `sync()` updates the file correctly and emits
no `configChanged()` at all — which is indistinguishable from a watcher that was
never connected, and sends you looking in the wrong place entirely. When testing
live reload by hand, `kwriteconfig6` needs `--notify` for the same reason.

## Roadmap

| | Milestone | Status |
|---|---|---|
| M0 | ScreenShot2 capture | done |
| M1 | Layer-shell region selection, DPI-correct | done |
| M2 | Tesseract → clipboard, raw mode | done |
| M3 | Preprocessing + fixture test corpus | done |
| M4 | Layout classification; code/prose/table modes | done |
| M5 | Prose post-correction (dictionary hyphen check) | done |
| M6 | Tier 2: PP-OCRv6_small via ONNX Runtime, CPU | done |
| M7a | Settings, config dialog, live reload | done |
| M7b | Packaging, systemd unit | next |

Two entries differ from what earlier versions of this file promised, and the
reasons are worth stating. **M5 shipped half of what it planned**: a
confusion-set corrector was built, measured against the corpus, found to fire
zero times on real data, and deleted. The dictionary hyphen check survived
because it moves a score. **M6 is CPU, not GPU, and PP-OCRv6 rather than v5** —
the CUDA dependency was measured at ~6 GiB for no gain at this model size, and
v6 superseded v5 in June 2026.

Preprocessing does **not** binarise by default, and the honest version of that
claim is narrower than this file used to make it. Otsu thresholding costs 0.0054
on the corpus at the default 3× upscale, which is why it is off — but it *helps*
by 0.0022 at 2×. So it is "not helpful at the default", not "harmful on screen
text in general". Upscaling and polarity detection are where the wins are.

## Contributing

Unit-testable code is written test-first. `capture/`, `overlay/`, the config
dialog and the daemon controller are the exceptions — they need a live
compositor or live engines, and are covered by the `--capture-test` and
`--select-test` modes plus manual checks.

Accuracy changes are held to the corpus rather than to judgement. A change that
moves no score has not earned its place, and lowering a fixture's floor to get
a green run is how a regression net rots — if a floor looks wrong, read the
diff instead.

```bash
ctest --test-dir build --output-on-failure
```

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
