# TXT2PDF (v1.9.2) — THE SPARTAN PDF GENERATOR

> "Turn plain text into structured PDFs without asking for permission.  
> Now the objects come out folded."

`txt2pdf` is a zero-dependency, command-line utility written in pure C11 that converts plain text files into structured PDF 1.5 documents by default, with zlib stream compression and native PDF object streams powered by libdeflate compiled straight into the static binary.

Legacy PDF 1.4 output remains available with `--pdf14`.


## Core Features

- **Pure C11**: No C++ runtime, no external runtime dependencies.
- **Tiny & Static**: Self-contained static binary using musl libc (~114–137 KB stripped).
- **PDF 1.5 By Default**: Generates PDF 1.5 output with modern structural compression.
- **Native Object Streams**: Structural non-stream objects are grouped into `/ObjStm` objects.
- **Native Cross-Reference Streams**: Uses binary `/XRef` streams with `/W [1 4 2]` in PDF 1.5 object-stream mode.
- **No External Post-Processing**: Object streams are generated natively; no external `qpdf` is required.
- **Zlib Stream Compression**: Page content streams, ToUnicode CMap and FontFile2 are compressed with libdeflate (`/Filter /FlateDecode`). Enabled by default at level 12; `--no-compress` disables it.
- **Rich PDF Output**: Valid PDF 1.5 documents by default with outlines and internal links. Legacy PDF 1.4 output is available with `--pdf14`.
- **Clean Internal Links**: TOC entries are rendered as blue text without underline, with codepoint-exact clickable rectangles.
- **Base14 Fonts**: Built-in support for Courier, Helvetica, and Times-Roman.
- **Native Image Passthrough**: JPEG and PNG embedding without recompression, with DPI awareness.
- **Image Centering**: Per-image `[CENTER]` tag and global `--center-images` option.
- **Image DPI Awareness**: JPEG JFIF density and PNG pHYs chunks are read automatically; `-res N` overrides the display DPI.
- Unicode CJK Fonts: Embedded TrueType font support via `--font FILE.ttf` (auto-detected since v1.9.2: only used when the document contains real CJK text).
- **TrueType Subsetting**: Embeds only the glyphs used by the document.
- **External URL Links**: Automatic detection of `http://`, `https://`, `ftp://` and `www.` URLs, rendered as blue underlined clickable links.
- **Multi-Architecture**: Cross-compiles cleanly to RISC-V, x86_64, and ARM with zero code changes.
- **Deterministic Builds**: Reproducible binaries with `SOURCE_DATE_EPOCH` and `ZERO_AR_DATE`.


## What Is New In v1.9.2
Version 1.9.2 makes `--font` smart: the Unicode TrueType pipeline is only activated when the document really contains CJK text.

### Main changes
- `--font` auto-detection: `--font FILE.ttf` now scans the document for real CJK codepoints (ideographs, kana, hangul, fullwidth forms) before enabling the TrueType pipeline.
- Documents without CJK text keep the Base14 font (Courier by default, or the one selected with `--sans`/`--serif`): nothing is embedded, streams stay small and fixed-width ASCII art stays perfectly aligned.
- Documents with CJK text embed the selected TrueType font exactly as before (subsetted, Identity-H, ToUnicode).
- Non-CJK characters outside WinAnsi (e.g. an isolated emoji) are printed as '?' in Base14 mode, as in previous versions.

### New --debug diagnostics
    [debug] No CJK chars detected: keeping Base14 font (--font ignored)
    [debug] CJK chars detected, embedding font: <resolved-path>

### Example
No CJK text: keeps Courier, minimal size, ASCII art aligned:
./txt2pdf --font NotoSansJP-Regular.ttf report.txt report.pdf

Document with Japanese text: embeds subsetted Noto Sans JP:
./txt2pdf --font NotoSansJP-Regular.ttf roadmap.txt roadmap.pdf

## What Is New In v1.9.1

Version 1.9.1 adds image DPI awareness, image centering support, and fixes several correctness issues.

### New features

- **Image DPI awareness**: JPEG JFIF density and PNG pHYs chunks are now read automatically, so images that declare their own DPI are sized correctly without forcing a global value.
- **`-res N` / `--image-dpi N`**: Override the display DPI for all images (e.g. 72, 96, 300).
- **`--center-images`**: Center all `[IMG]` blocks horizontally.
- **`[CENTER][IMG]file[/IMG][/CENTER]`**: Per-image centering tag, overrides `--center-images` when both are used.
- **Accurate file size statistics**: Console summary now reports the real output size and passthrough image bytes.
- **Enhanced debug output**: `--debug` now prints the centered flag and display size for every detected image.

### Bug fixes

- Fixed a critical argument parsing bug where `--level` was ignored due to a duplicated empty `else-if` block. Passing `--level 9` now correctly sets the compression level.
- Added an early `access()` check to validate input file readability before processing, providing clearer error messages instead of a generic "Cannot read".

### Codebase cleanup

- Removed dead/placeholder code for image re-encoding (`-q`/`--quality`, `--copy-image`, `--no-copy-image`) to maintain a strictly spartan codebase. Users should pre-process images externally for resizing or compression.

### Console output example

```
Compression: zlib level 12 (libdeflate)
Streams: 7697 -> 2161 bytes (71.9% saved)
Images (passthrough, not compressed): 10824 bytes
Final File size: 13583 bytes
```


## What Is New In v1.9.0

Version 1.9.0 adds native PDF 1.5 object streams and native cross-reference streams, reducing structural PDF overhead without external tools.

### Main additions

- Native PDF 1.5 output by default.
- Native object streams (`/ObjStm`) for structural PDF objects.
- Native cross-reference streams (`/XRef`) with binary `/W [1 4 2]` entries.
- Object streams and xref streams are compressed with libdeflate when useful.
- New options: `--pdf15`, `--pdf14`, `--object-streams=generate`, `--object-streams=disable`, `--objstm`, `--no-object-streams`.
- Default behavior: PDF 1.5 + object streams + no linearization.
- Linearization is intentionally not generated. Linearization optimizes Fast Web View, not minimum file size.
- Classic PDF 1.4 output remains available with `--pdf14`.
- Generated PDF 1.5 output passes `qpdf --check`.
- Internal TOC links rendered as plain blue text without underline; annotations use an invisible border (`/Border [0 0 0]`) and no `/BS` underline.
- Codepoint-aware Courier metrics: clickable `/Rect` boxes end exactly at the last printed letter (trailing spaces, tabs and dots trimmed); UTF-8 accents no longer stretch link boxes.
- Parser: glued indexes (`1.what is txt2pdf?`) and mixed-case section titles accepted natively; the space after the numeric dot is optional.
- New `--debug` diagnostics: unparsable TOC lines, TOC entries without a matching section, and section headers without any TOC link (reconciles Bookmarks vs Internal links counts).

### Example PDF 1.5 impact

Spanish manual from Baresnap:

| Mode | Size (bytes) |
| --- | --- |
| txt2pdf v1.8 PDF 1.4 | 373,965 |
| qpdf --object-streams=generate | 291,972 |
| txt2pdf v1.9 native PDF 1.5 | 289,417 |

In this test, native `txt2pdf` v1.9 output is slightly smaller than the same document post-processed with `qpdf`.


## What Is New In v1.8.0

Version 1.8.0 added zlib stream compression via libdeflate, external URL support, and multi-architecture builds.

- Added zlib stream compression using libdeflate (`/Filter /FlateDecode`).
- Compression enabled by default at level 12.
- Added `--level N` to choose the compression level.
- Added `--no-compress` to generate uncompressed PDFs as in v1.7.2.
- Compressed streams: page content streams, ToUnicode CMap and FontFile2.
- Images are never recompressed.
- Added external URL support: `http://`, `https://`, `ftp://` and `www.` links.
- URLs become clickable `/URI` link annotations, rendered in blue with underline.
- Added `--version` / `-V` and a cleaner `--help` / `-h`.
- Unknown options produce an error instead of being ignored.
- Multi-architecture build script: `amd64`, `arm64`, `riscv64`.
- Deterministic builds with `SOURCE_DATE_EPOCH` and `ZERO_AR_DATE`.


## Quick Start

### PDF 1.5 / Object Streams

```bash
# Default: PDF 1.5 + object streams
./txt2pdf input.txt output.pdf

# Legacy PDF 1.4
./txt2pdf --pdf14 input.txt output.pdf

# Disable object streams
./txt2pdf --object-streams=disable input.txt output.pdf

# Force object streams explicitly
./txt2pdf --object-streams=generate input.txt output.pdf
```

### Compression

```bash
# Default: compression ON, zlib level 12
./txt2pdf input.txt output.pdf

# Custom level (1 = fastest, 12 = smallest)
./txt2pdf --level 6 input.txt output.pdf

# Disable compression (classic v1.7.2 stream behavior)
./txt2pdf --no-compress input.txt output.pdf
```

At the end of each run a summary is printed:

```
Compression: zlib level 12 (libdeflate)
Streams: 7697 -> 2161 bytes (71.9% saved)
Images (passthrough, not compressed): 10824 bytes
Final File size: 13583 bytes
```

### Images

```bash
# Force display DPI for all images
./txt2pdf -res 96 input.txt output.pdf

# Center all images horizontally
./txt2pdf --center-images input.txt output.pdf

# Combine both
./txt2pdf -res 96 --center-images input.txt output.pdf
```

Image tags in the source text:

```
[IMG]photo.jpg[/IMG]
[IMG]diagram.png[/IMG]
[CENTER][IMG]logo.jpg[/IMG][/CENTER]
```

The `[CENTER]` tag overrides `--center-images` when both are used.

### CJK / Unicode Fonts

```bash
./txt2pdf --font NotoSansJP-Regular.ttf roadmap.txt roadmap.pdf
./txt2pdf --font NotoSansSC-Regular.ttf input.txt output.pdf
./txt2pdf --font NotoSansKR-Regular.ttf input.txt output.pdf
```

**Important:**

- The font must be an individual TrueType `.ttf` file.
- OpenType/CFF `.otf` files are not supported yet.
- TrueType collections `.ttc` are not supported yet.
- When `--font` is used, it overrides `--sans`, `--serif` and `--mono`.
- Since v1.9.2, `--font` is auto-detected: the TrueType pipeline only activates when the document contains real CJK codepoints. Without CJK text the Base14 font is kept (no embedding, ASCII art aligned).


## URL Links

External URLs in the text are automatically detected and converted to clickable links:

- `http://example.com`
- `https://github.com/johna124`
- `ftp://files.example.org`
- `www.example.com` (automatically prefixed with `https://`)

URLs are rendered in blue with underline (HTML-style) and are clickable in any PDF viewer. Trailing punctuation (`. , ; : ! ?`) is automatically stripped from the link.

**Example:**

```
Also http://example.com and www.example.org are working.
```

All detected URLs become clickable blue underlined links.

Internal TOC links use a different, cleaner style: blue text without underline, with a click rectangle measured to the exact last character.


## Building

```bash
./compile.sh platform or all
```

The script:

- Builds libdeflate as a static library.
- Uses the libdeflate Makefile if present; otherwise compiles the `libdeflate/lib` sources directly.
- Does not require CMake.
- Verifies the library exports the required symbols.
- Links `txt2pdf` as a fully static, stripped binary.

**Size-oriented flags:**

```
-Os
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
-fno-unwind-tables
-Wl,--build-id=none
-s
```


## Multi-Architecture Builds

```bash
# Build for host architecture (auto-detected)
./compile.sh

# Build for specific architecture
./compile.sh amd64
./compile.sh arm64
./compile.sh riscv64

# Build all three at once
./compile.sh all
```

The script auto-downloads musl.cc toolchains if missing:

- `x86_64-linux-musl-cross.tgz`
- `aarch64-linux-musl-cross.tgz`
- `riscv64-linux-musl-cross.tgz`

Toolchains are cached in `.toolchains/` and reused.


## Deterministic Builds

Builds are reproducible with:

```bash
SOURCE_DATE_EPOCH=1179146266
ZERO_AR_DATE=1
```

Same input produces the same output hash every time.


## Binary Sizes (actual)

| Architecture | Size |
| --- | --- |
| amd64 | 132848 bytes (~129 KB) |
| arm64 | 137352 bytes (~134 KB) |
| riscv64 | 116792 bytes (~114 KB) |

RISC-V is usually the smallest thanks to RVC (compressed 16-bit instructions).


## Why txt2pdf? (Comparison)

If you just need to generate a structured PDF from raw text, code, or logs, using massive office suites or complex pipelines is like using a rocket launcher to swat a fly. 

Here is how `txt2pdf` compares to other common tools in the ecosystem:

| Feature | `txt2pdf` | Ghostscript (`gs`) | LibreOffice (`--headless`) | `qpdf` (post-processor) | `enscript` / `a2ps` | `pandoc` (+ engine) |
| --- | --- | --- | --- | --- | --- | --- |
| **Primary Purpose** | Direct TXT to PDF generation | PostScript/PDF interpreter & optimizer | Full office suite (CLI mode) | PDF manipulation & optimization | Text to PostScript/PDF | Universal document conversion |
| **Output File Size (Pure Text)** | **Best (~23 KB)** (No structural bloat) | **Medium (~40-41 KB)** (Fixed overhead toll) | **Large (~46 KB)** (Heavy metadata overhead) | Varies (Depends on input PDF) | **Medium** (Requires secondary conversion) | **Large** (Engine-dependent bloat) |
| **Binary Size** | **~114–137 KB** (static, stripped) | ~5–10 MB | ~300–500 MB+ (Full suite) | ~2–5 MB (dynamic) + libqpdf | ~500 KB + requires Ghostscript | ~50–200 MB+ (Haskell/Python runtime) |
| **Runtime Dependencies** | **None** (pure C11 + embedded libdeflate) | `libgs`, `libjpeg`, `zlib`, fonts | Huge Java/UNO runtime, system fonts | `libqpdf`, `libjpeg`, `zlib` | Ghostscript (for PDF), `libiconv` | LaTeX, `wkhtmltopdf`, or WebKit |
| **Execution Speed** | **Blazing fast** (single-pass, near-zero overhead) | Moderate (Heavy processing stream) | Very slow (Heavy startup and JVM/UNO init) | Fast, but requires a second I/O pass | Moderate (Legacy codebase) | Slow startup, heavy parsing overhead |
| **PDF 1.5 Object Streams** | **Yes** (generated natively in one pass) | Yes (Destroys links on compressed presets) | Yes | Yes (But requires running it *after* generation) | No | Varies by output engine |
| **External URL Links** | **Yes** (auto-detected, blue + underline) | N/A (Does not parse text natively) | Yes (If formatted as links) | N/A (Does not parse text) | No | Yes (via Markdown/HTML) |
| **Internal TOC Links** | **Yes** (blue text, no underline, exact click rects) | N/A (Strips them by default on extreme compression) | Yes (If explicitly mapped) | N/A | No | Yes (But requires specific syntax) |
| **Image Passthrough** | **Yes** (JPEG/PNG embedded untouched) | Re-encodes/Downsamples based on presets | Compresses/Converts internally | Can optimize, but doesn't generate from text | Limited/Complex | Yes (But heavy processing) |
| **Deterministic Builds** | **Yes** (`SOURCE_DATE_EPOCH`, `ZERO_AR_DATE`) | No | No | No | No | No |
| **Target Audience** | Systems hackers, minimalists, embedded, CI/CD | PDF developers, print pre-flight | Desktop users, enterprise scripting | PDF developers, archivists | Legacy Unix sysadmins | Academics, technical writers |


### The `qpdf` Advantage
Historically, minimizing PDF structural overhead required a two-step pipeline:
```bash
./txt2pdf input.txt temp.pdf
qpdf --object-streams=generate temp.pdf final.pdf
rm temp.pdf
```
With v1.9.0+, `txt2pdf` generates native PDF 1.5 object streams (`/ObjStm`) and cross-reference streams (`/XRef`) **directly inside the static binary**. You get the exact same structural size benefits as `qpdf`, but in a single, deterministic pass, with zero external dependencies and no temporary files.

### The Spartan Advantage
While tools like Pandoc, LibreOffice, or WebKit-based converters are excellent for rich document design, they treat plain text as a raw, unstructured block that must first be wrapped in Markdown, LaTeX, or HTML. 

`txt2pdf` cuts the bloat entirely. It natively parses your raw text stream, automatically detects document hierarchy, and embeds cross-linked internal navigation and PDF outlines out of the box. It is designed to run inside ultra-low-power RISC-V/ARM64 dev-boards, minimal Docker containers, or bare-metal environments where modern runtimes simply choke.

With v1.7.2, it gained subsetted TrueType embedding for real Unicode CJK text. With v1.8.0, it squeezed every text stream through libdeflate at level 12 and turned every URL into a clickable link. With v1.9.0, it eliminated the need for `qpdf` by generating native PDF 1.5 object streams directly. And with v1.9.1, it added image DPI awareness, per-image centering, accurate file size reporting, and a cleaner, strictly spartan codebase. And with v1.9.2, `--font` became smart: the Unicode TrueType pipeline only activates for documents with real CJK text, keeping ASCII-heavy reports in Courier with zero font embedding.


## The Spartan Advantage

While Adobe Acrobat and LibreOffice are excellent for rich document design, they treat plain text as a raw, unstructured block. Generating table of contents, clickable navigation, or document outlines requires tedious manual formatting. Even Pandoc forces you to rewrite your files into Markdown or LaTeX syntax first.

`txt2pdf` cuts the bloat entirely. It natively parses your raw text stream, automatically detects document hierarchy, and embeds cross-linked internal navigation and PDF outlines out of the box, all while running inside an ultra-low-power RISC-V/ARM64 dev-board or bare-metal environment using minimal CPU and RAM.

With v1.7.2, `txt2pdf` gained subsetted TrueType embedding for real Unicode CJK text. With v1.8.0, it squeezed every text stream through libdeflate at level 12 and turned every `http://`/`https://`/`www.` URL into a clickable blue underlined link. With v1.9.0, it generates native PDF 1.5 object streams and cross-reference streams directly inside the static binary, reducing structural PDF overhead without requiring `qpdf` or any external post-processing tool, and renders internal TOC links as clean blue text with codepoint-exact click rectangles. With v1.9.1, it adds image DPI awareness, per-image and global centering, accurate file size statistics, and cleans up the codebase of unused placeholder options. And with v1.9.2, `--font` became smart: the Unicode TrueType pipeline only activates for documents with real CJK text, keeping ASCII-heavy reports in Courier with zero font embedding.


## How It Works (The Core Parser)

Unlike standard text editors that treat files as a flat sequence of characters, `txt2pdf` features a smart, single-pass streaming parser. It scans your raw text and automatically structures the output based on predictable plaintext patterns:

- **Automatic Outlines**: Identifies document hierarchies by analyzing separators and numbered headers (any case), injecting them into the native PDF interactive sidebar.
- **Smart Internal Links**: Any detected structural reference within the TOC block is converted into a clickable internal hyperlink, rendered as blue text without underline and with an exact click rectangle.
- **Smart External Links**: URLs in the text are detected, underlined in blue, and converted to `/URI` annotations.
- **Centered Block Images**: `[CENTER][IMG]file[/IMG][/CENTER]` tags center images horizontally between the page margins.
- **Stream Processing**: Reads and converts text efficiently without heavyweight document frameworks.
- **Compressed Streams**: Content streams, ToUnicode CMap and FontFile2 are written through libdeflate (zlib format) and declared with `/Filter /FlateDecode`.
- **PDF 1.5 Structural Compression**: When object streams are enabled, structural non-stream objects are grouped into an `/ObjStm` object and referenced by a binary `/XRef` stream.


## Debug Mode

```bash
./txt2pdf --debug input.txt output.pdf
```

It reports:

- detected encoding
- selected font
- font auto-detection decision (CJK pipeline vs Base14)
- compression mode and level
- PDF version and object-stream mode
- lines marked as part of a contents block
- detected section headers
- skipped uppercase numbered lines
- detected internal links
- TOC lines that cannot be parsed as numbered entries
- TOC entries without a matching section
- section headers without any TOC link pointing to them
- detected images and image placements (including centered flag and display size)
- CJK glyph count and subset size (in CJK mode)

Use it when the Bookmarks and Internal links counts do not match: it prints exactly which orphan TOC entry or orphan section header breaks the pair.


## Hardware Footprint and Efficiency

Built for systems hackers and ultra-low-power environments, `txt2pdf` is designed to run where modern runtimes choke:

- **Memory Usage**: Very low RAM usage for normal text documents.
- **Storage Footprint**: ~114–137 KB static binary when stripped.
- **CPU Overhead**: Near-zero parsing overhead for text layout and PDF object generation.
- **Compression Tradeoff**: Level 12 compression favors size over speed; use `--level 1..6` on constrained devices if generation speed matters more than PDF size.
- **CJK Mode**: Memory usage depends mainly on the source TTF file and the number of glyphs used.


## Known Limitations

- Object streams require PDF 1.5. Use `--pdf14` for legacy PDF 1.4 output.
- No linearization (Fast Web View) is generated.
- No custom page sizes.
- No custom margins.
- No multi-column layout.
- No inline images.
- PNG support is limited to simple 8-bit non-interlaced images.
- URLs are only linked when they fit entirely inside one visual line.
- Base14 mode is limited to WinAnsiEncoding.
- Since v1.9.2, `--font` is ignored when the document has no CJK codepoints; characters outside WinAnsi (emoji, symbols) are printed as '?' in Base14 mode.
- CJK mode requires an individual `.ttf` file; `.otf` and `.ttc` are not supported yet.
- Images are embedded untouched; no re-encoding is performed. To resize or recompress, do it on the source file before running `txt2pdf`.


## License

- **Source Code**: GPLv3
- **Documentation / Manual**: GFDL v1.3
- **libdeflate**: MIT license, Copyright 2016 Eric Biggers

[https://github.com/johna124/txt2pdf](https://github.com/johna124/txt2pdf)
