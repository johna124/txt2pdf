# TXT2PDF (v1.9.0) — THE SPARTAN PDF GENERATOR

> "Turn plain text into structured PDFs without asking for permission.
> Now the objects come out folded."

`txt2pdf` is a zero-dependency, command-line utility written in pure C11 that converts plain text files into structured PDF 1.5 documents by default, with zlib stream compression and native PDF object streams powered by libdeflate compiled straight into the static binary.

Legacy PDF 1.4 output remains available with `--pdf14`.

---

## Core Features

- **Pure C11:** No C++ runtime, no external runtime dependencies.
- **Tiny & Static:** Self-contained static binary using musl libc (~90–110 KB stripped).
- **PDF 1.5 By Default:** Generates PDF 1.5 output with modern structural compression.
- **Native Object Streams:** Structural non-stream objects are grouped into `/ObjStm` objects.
- **Native Cross-Reference Streams:** Uses binary `/XRef` streams with `/W [1 4 2]` in PDF 1.5 object-stream mode.
- **No External Post-Processing:** Object streams are generated natively; no external `qpdf` is required.
- **Zlib Stream Compression:** Page content streams, ToUnicode CMap and FontFile2 are compressed with libdeflate (`/Filter /FlateDecode`). Enabled by default at level 12; `--no-compress` disables it.
- **Rich PDF Output:** Valid PDF 1.5 documents by default with outlines and internal links. Legacy PDF 1.4 output is available with `--pdf14`.
- **Clean Internal Links:** TOC entries are rendered as blue text **without underline**, with codepoint-exact clickable rectangles.
- **Base14 Fonts:** Built-in support for Courier, Helvetica, and Times-Roman.
- **Native Image Passthrough:** JPEG and PNG embedding without recompression.
- **Unicode CJK Fonts:** Embedded TrueType font support via `--font FILE.ttf`.
- **TrueType Subsetting:** Embeds only the glyphs used by the document.
- **External URL Links:** Automatic detection of `http://`, `https://`, `ftp://` and `www.` URLs, rendered as blue underlined clickable links.
- **Multi-Architecture:** Cross-compiles cleanly to RISC-V, x86_64, and ARM with zero code changes.
- **Deterministic Builds:** Reproducible binaries with `SOURCE_DATE_EPOCH` and `ZERO_AR_DATE`.

---

## What Is New In v1.9.0

Version 1.9.0 adds native PDF 1.5 object streams and native cross-reference streams, reducing structural PDF overhead without external tools.

### Main additions

- Native PDF 1.5 output by default.
- Native object streams (`/ObjStm`) for structural PDF objects.
- Native cross-reference streams (`/XRef`) with binary `/W [1 4 2]` entries.
- Object streams and xref streams are compressed with libdeflate when useful.
- New options:
  - `--pdf15`
  - `--pdf14`
  - `--object-streams=generate`
  - `--object-streams=disable`
  - `--objstm`
  - `--no-object-streams`
- Default behavior: **PDF 1.5 + object streams + no linearization**.
- Linearization is intentionally not generated. Linearization optimizes Fast Web View, not minimum file size.
- Classic PDF 1.4 output remains available with `--pdf14`.
- Generated PDF 1.5 output passes `qpdf --check`.
- **Internal TOC links rendered as plain blue text without underline**; annotations use an invisible border (`/Border [0 0 0]`) and no `/BS` underline.
- **Codepoint-aware Courier metrics:** clickable `/Rect` boxes end exactly at the last printed letter (trailing spaces, tabs and dots trimmed); UTF-8 accents no longer stretch link boxes.
- **Parser:** glued indexes (`1.what is txt2pdf?`) and mixed-case section titles accepted natively; the space after the numeric dot is optional.
- **New `--debug` diagnostics:** unparsable TOC lines, TOC entries without a matching section, and section headers without any TOC link (reconciles Bookmarks vs Internal links counts).

### Example PDF 1.5 impact

Spanish manual from Baresnap:

| Mode | Size (bytes) |
| --- | --- |
| txt2pdf v1.8 PDF 1.4 | 373,965 |
| `qpdf --object-streams=generate` | 291,972 |
| txt2pdf v1.9 native PDF 1.5 | **289,417** |

In this test, native `txt2pdf` v1.9 output is slightly smaller than the same document post-processed with `qpdf`.

---

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

---

## PDF 1.5 Quick Start

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

## Compression Quick Start

```bash
# Default: compression ON, zlib level 12
./txt2pdf input.txt output.pdf

# Custom level (1 = fastest, 12 = smallest)
./txt2pdf --level 6 input.txt output.pdf

# Disable compression (classic v1.7.2 stream behavior)
./txt2pdf --no-compress input.txt output.pdf
```

At the end of each run a summary line is printed:

```
Compression: zlib level 12 (libdeflate) | streams 824314 -> 261247 bytes (68.3% saved)
```

---

## URL Links

External URLs in the text are automatically detected and converted to clickable links:

- `http://example.com`
- `https://github.com/johna124`
- `ftp://files.example.org`
- `www.example.com` (automatically prefixed with `https://`)

URLs are rendered in blue with underline (HTML-style) and are clickable in any PDF viewer. Trailing punctuation (`. , ; : ! ?`) is automatically stripped from the link.

Example:

```
Also http://example.com and www.example.org are working.
```

All detected URLs become clickable blue underlined links.

Internal TOC links use a different, cleaner style: **blue text without underline**, with a click rectangle measured to the exact last character.

---

## CJK Quick Start

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

---

## Building

```bash
./compile.sh
```

The script:

- Builds libdeflate as a static library.
- Uses the libdeflate Makefile if present; otherwise compiles the `libdeflate/lib` sources directly.
- Does not require CMake.
- Verifies the library exports the required symbols.
- Links `txt2pdf` as a fully static, stripped binary.

Size-oriented flags:

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

## Binary Sizes (typical)

| Architecture | Size |
| --- | --- |
| amd64 | ~117 KB |
| arm64 | ~122 KB |
| riscv64 | ~102 KB |

RISC-V is usually the smallest thanks to RVC (compressed 16-bit instructions).

---

## Why txt2pdf? (Comparison)

If you just need to generate a structured PDF from raw text, code, or logs, using massive office suites or proprietary software is like using a rocket launcher to swat a fly.

| Feature | Adobe Acrobat Pro | LibreOffice / Pandoc | txt2pdf |
| --- | --- | --- | --- |
| Primary Interface | Heavy GUI | CLI / GUI (Java/Python) | Minimalist CLI |
| Dependencies | Massive proprietary ecosystem | Huge (hundreds of MBs / runtimes) | None at runtime (pure C11 + embedded libdeflate) |
| Binary Size | Gigabytes | 100 MB to 1 GB+ | ~90–110 KB static binary |
| Architecture Support | x86_64, ARM64 (limited) | x86_64, ARM64 | x86_64, ARM64, RISC-V |
| Execution Speed | Slow startup | Moderate (heavy parsing overhead) | Blazing fast |
| Plain Text Outlines / Links | No (manual styling) | No (Markdown/LaTeX syntax) | Yes (auto-generated from raw text) |
| Stream Compression | Yes | Yes | Yes (zlib via libdeflate, level 12 default) |
| PDF Object Streams | Yes | Varies | Yes (native `/ObjStm` and `/XRef`) |
| External URL Links | Yes | Yes | Yes (auto-detected, blue + underline) |
| Internal TOC Links | Yes | Yes | Yes (blue text, no underline, exact click rects) |
| Unicode CJK Embedding | Yes | Yes | Yes (with embedded TTF subset) |
| Deterministic Builds | No | No | Yes (`SOURCE_DATE_EPOCH`) |
| Target Audience | Enterprise / end-users | General document automation | Systems hackers, minimalists, embedded systems |

## The Spartan Advantage

While Adobe Acrobat and LibreOffice are excellent for rich document design, they treat plain text as a raw, unstructured block. Generating table of contents, clickable navigation, or document outlines requires tedious manual formatting. Even Pandoc forces you to rewrite your files into Markdown or LaTeX syntax first.

`txt2pdf` cuts the bloat entirely. It natively parses your raw text stream, automatically detects document hierarchy, and embeds cross-linked internal navigation and PDF outlines out of the box, all while running inside an ultra-low-power RISC-V/ARM64 dev-board or bare-metal environment using minimal CPU and RAM.

With v1.7.2, `txt2pdf` gained subsetted TrueType embedding for real Unicode CJK text. With v1.8.0, it squeezed every text stream through libdeflate at level 12 and turned every `http://`/`https://`/`www.` URL into a clickable blue underlined link. With v1.9.0, it generates native PDF 1.5 object streams and cross-reference streams directly inside the static binary, reducing structural PDF overhead without requiring `qpdf` or any external post-processing tool, and renders internal TOC links as clean blue text with codepoint-exact click rectangles.

---

## How It Works (The Core Parser)

Unlike standard text editors that treat files as a flat sequence of characters, `txt2pdf` features a smart, single-pass streaming parser. It scans your raw text and automatically structures the output based on predictable plaintext patterns:

- **Automatic Outlines:** Identifies document hierarchies by analyzing separators and numbered headers (any case), injecting them into the native PDF interactive sidebar.
- **Smart Internal Links:** Any detected structural reference within the TOC block is converted into a clickable internal hyperlink, rendered as blue text without underline and with an exact click rectangle.
- **Smart External Links:** URLs in the text are detected, underlined in blue, and converted to `/URI` annotations.
- **Stream Processing:** Reads and converts text efficiently without heavyweight document frameworks.
- **Compressed Streams:** Content streams, ToUnicode CMap and FontFile2 are written through libdeflate (zlib format) and declared with `/Filter /FlateDecode`.
- **PDF 1.5 Structural Compression:** When object streams are enabled, structural non-stream objects are grouped into an `/ObjStm` object and referenced by a binary `/XRef` stream.

## Debug Mode

```bash
./txt2pdf --debug input.txt output.pdf
```

It reports:

- detected encoding
- selected font
- compression mode and level
- PDF version and object-stream mode
- lines marked as part of a contents block
- detected section headers
- skipped uppercase numbered lines
- detected internal links
- TOC lines that cannot be parsed as numbered entries
- TOC entries without a matching section
- section headers without any TOC link pointing to them
- detected images and image placements
- CJK glyph count and subset size (in CJK mode)

Use it when the **Bookmarks** and **Internal links** counts do not match: it prints exactly which orphan TOC entry or orphan section header breaks the pair.

---

## Hardware Footprint and Efficiency

Built for systems hackers and ultra-low-power environments, `txt2pdf` is designed to run where modern runtimes choke:

- **Memory Usage:** Very low RAM usage for normal text documents.
- **Storage Footprint:** ~102–122 KB static binary when stripped.
- **CPU Overhead:** Near-zero parsing overhead for text layout and PDF object generation.
- **Compression Tradeoff:** Level 12 compression favors size over speed; use `--level 1..6` on constrained devices if generation speed matters more than PDF size.
- **CJK Mode:** Memory usage depends mainly on the source TTF file and the number of glyphs used.

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
- CJK mode requires an individual `.ttf` file; `.otf` and `.ttc` are not supported yet.

---

## License

- **Source Code:** GPLv3
- **Documentation / Manual:** GFDL v1.3
- **libdeflate:** MIT license, Copyright 2016 Eric Biggers

<https://github.com/johna124/txt2pdf>
