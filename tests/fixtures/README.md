# Fixture corpus

Screenshots paired with the text they should produce. `test_fixtures` runs each
one through the same pipeline the daemon runs — `preprocess` → Tesseract →
`assemble` — and reports a per-fixture accuracy score.

The corpus is the accuracy roadmap. Every accuracy claim about this project
should be answerable by running it, and every real-world miss should end up here
as a new entry.

## Capturing a fixture

The overlay needs a live compositor, so fixtures are captured by hand.

```bash
cmake --build build -j$(nproc)
cp build/org.kde.textract.dev.desktop ~/.local/share/applications/
kbuildsycoca6

./build/bin/textract --select-test tests/fixtures/dark-terminal-htop.png
```

Drag over the region you want. The crop is written to that path.

Then write the text it *should* produce, byte for byte, next to it:

```bash
$EDITOR tests/fixtures/dark-terminal-htop.txt
```

Type what a careful human reading the screenshot would produce — not what
Tesseract currently produces. A fixture whose expected text was copied from the
output can never detect a regression, because it already encodes one.

Comparison normalises line endings, trailing whitespace on each line, and
leading/trailing blank lines. Interior blank lines and indentation are
significant.

## Registering it

Add an entry to `manifest.json`:

```json
{
    "fixtures": [
        {
            "name": "dark-terminal-htop",
            "image": "dark-terminal-htop.png",
            "expected": "dark-terminal-htop.txt",
            "langs": "eng",
            "layout": "raw",
            "minScore": 0.0,
            "notes": "Dark-mode terminal; exercises the polarity path."
        }
    ]
}
```

| Field | Required | Meaning |
|---|---|---|
| `name` | yes | Identifier in test output |
| `image` | yes | PNG, relative to this directory |
| `expected` | yes | UTF-8 text, relative to this directory |
| `langs` | no | Tesseract spec, e.g. `eng+msa`. Default `eng` |
| `layout` | no | `raw`, `code`, `prose`, `table`. Default `raw` |
| `minScore` | no | Regression floor in [0,1]. Default `0.0` |
| `notes` | no | What this fixture is here to catch |

`minScore` is a floor, not a target. Set it a little below the score the
fixture currently achieves, so the suite fails on a regression but a hard case
can live in the corpus while it is still being worked on. Raise it as accuracy
improves — that ratchet is the point.

A fixture whose `langs` are not installed is skipped, not failed.

## Coverage the spec asks for

- [ ] dark-mode terminal output — the polarity path
- [ ] two-column PDF prose — M4's hardest case
- [ ] Bahasa Melayu (`msa`) and CJK (`chi_sim`)
- [ ] tables and spreadsheet regions
- [ ] small-font UI text at 1.25 scale

## Running it

```bash
ctest --test-dir build -R test_fixtures --output-on-failure   # scores + gate
./build/bin/textract-fixture-report                           # 2x/3x/4x sweep
./build/bin/textract-fixture-report --binarize                # Otsu comparison
```

The sweep exists to settle the upscale default by measurement. The spec commits
to 3× on reasoning alone; once this corpus has real captures in it, run the
sweep and record the answer in the spec rather than leaving it asserted.

`TEXTRACT_FIXTURES=/path/to/manifest.json` points either tool at a corpus
outside the repository.
