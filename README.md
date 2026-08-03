# text-extractor

Screen text extraction for KDE Plasma on Wayland. Press a key, drag a region,
and the text is in your clipboard — the Windows 11 Snipping Tool's text
extractor, done natively for KWin.

```
hotkey → capture workspace → drag a region → OCR → clipboard
```

**Status:** working end to end for English text. This is milestone M2 of 8; the
accuracy pipeline (layout-aware assembly, post-correction, GPU tier) is not
built yet. See [Roadmap](#roadmap).

## Why, given Spectacle already does this

Plasma 6.6 added an "Extract Text" button to Spectacle. That covers the simple
case well. This project exists for the part it doesn't do: treating OCR output
as *structured* rather than as one flat string.

Code, prose, tables, and non-English text each want contradictory handling. Code
must never be autocorrected and needs its indentation preserved; prose benefits
from dictionary correction; tables need column reconstruction rather than words
run together. The plan is to detect which one you captured and reassemble
accordingly, with a heavier GPU engine on demand when the fast path isn't good
enough.

Today only raw extraction is implemented, so Spectacle remains the practical
choice unless you want to hack on this.

## Requirements

Built and tested on Arch (CachyOS), Plasma 6.7.3, KWin 6.7.3, Wayland.

```bash
sudo pacman -S --needed cmake extra-cmake-modules qt6-base layer-shell-qt \
                        tesseract tesseract-data-eng leptonica \
                        hunspell hunspell-en_us \
                        kglobalaccel kguiaddons kconfig knotifications \
                        ki18n kwindowsystem
```

Add language data as needed — `tesseract-data-msa`, `tesseract-data-chi_sim`,
`tesseract-data-ara`, and so on.

`hunspell` is a build dependency; the dictionaries are not. Prose
post-correction looks for `en_US.{aff,dic}` at runtime and quietly disables
itself when it finds none, so a machine without `hunspell-en_us` still
extracts text — it just does not second-guess it.

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
The recognised text lands on your clipboard.

Rebind it in *System Settings → Shortcuts → textract*. The default avoids
modifier combinations because most are already taken by Plasma.

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
├── ocr/         OcrEngine interface + Tesseract implementation
├── analyze/     what kind of region is this
├── assemble/    words → text, one branch per kind
├── correct/     Hunspell-backed prose post-correction
├── clipboard/   KSystemClipboard wrapper
└── app/         daemon, global shortcut, notifications
```

Anything that can be a pure function is one. `capture/` and `overlay/` need a
live compositor and are verified by hand; the rest is pure data transformation
with real unit tests, which is what makes accuracy work iterable.

The daemon is resident rather than fire-and-exit for two reasons: Tesseract's
`Init()` costs ~240ms and would otherwise be paid on every capture, and on
Wayland **clipboard contents die with the client that set them** — a one-shot
tool would copy text that vanishes the moment it exits.

## Gotchas worth knowing

Four things cost real debugging time here. Documented in case they save someone
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

## Roadmap

| | Milestone | Status |
|---|---|---|
| M0 | ScreenShot2 capture | done |
| M1 | Layer-shell region selection, DPI-correct | done |
| M2 | Tesseract → clipboard, raw mode | done |
| M3 | Preprocessing + fixture test corpus | done |
| M4 | Layout classification; code/prose/table modes | done |
| M5 | Prose post-correction (confusion-set + dictionary) | done |
| M6 | Tier 2: PP-OCRv5 via ONNX Runtime, GPU | next |
| M7 | Packaging, config UI | planned |

Preprocessing deliberately will **not** binarise by default. Otsu thresholding is
standard advice for scanned documents and tends to hurt screen text, which is
subpixel-antialiased and often light-on-dark; Tesseract 5's LSTM engine already
does its own adaptive thresholding. Upscaling and polarity detection are where
the wins are.

## Contributing

Unit-testable code is written test-first. `capture/` and `overlay/` are the
exception — they need a live compositor, and are covered by the `--capture-test`
and `--select-test` modes plus manual checks.

```bash
ctest --test-dir build --output-on-failure
```

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
