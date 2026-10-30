# TXT2PDF (v1.8.0) — THE SPARTAN PDF GENERATOR

`txt2pdf` is a zero-dependency, command-line utility written in pure C11 that converts plain text files into structured PDF 1.4 documents, with **zlib stream compression powered by libdeflate** compiled straight into the static binary.

## Core Features

- **Pure C11:** No C++ runtime, no external runtime dependencies.
- **Tiny & Static:** Self-contained static binary using musl libc (~90–110 KB stripped).
- **Zlib Stream Compression:** Page content streams, ToUnicode CMap and FontFile2 are compressed with libdeflate (`/Filter /FlateDecode`). Enabled by default at level 12; `--no-compress` disables it.
- **Rich PDF Output:** Valid PDF 1.4 documents with outlines and internal links.
- **Base14 Fonts:** Built-in support for Courier, Helvetica, and Times-Roman.
- **Native Image Passthrough:** JPEG and PNG embedding without recompression.
- **Unicode CJK Fonts:** Embedded TrueType font support via `--font FILE.ttf`.
- **TrueType Subsetting:** Embeds only the glyphs used by the document.
- **External URL Links:** Automatic detection of `http://`, `https://`, `ftp://` and `www.` URLs, rendered as blue underlined clickable links.
- **Multi-Architecture:** Cross-compiles cleanly to RISC-V, x86_64, and ARM with zero code changes.
- **Deterministic Builds:** Reproducible binaries with `SOURCE_DATE_EPOCH` and `ZERO_AR_DATE`.

## What Is New In v1.8.0

Version 1.8.0 adds zlib stream compression via libdeflate, external URL support, and multi-architecture builds.

Main additions:

- Added zlib stream compression using libdeflate (`/Filter /FlateDecode`).
- Compression is **enabled by default** at level 12 (libdeflate range: 1–12).
- Added `--level N` to choose the compression level.
- Added `--no-compress` to generate uncompressed PDFs as in v1.7.2.
- `--compress` is still accepted for scripting (it is already the default).
- Compressed streams: page content streams, ToUnicode CMap and FontFile2.
- Each stream is checked: it is only compressed when the result is smaller.
- Images (JPEG/PNG) are never recompressed.
- Added external URL support: `http://`, `https://`, `ftp://` and `www.` links found in the text become clickable `/URI` link annotations, rendered in blue with underline.
- Added `--version` / `-V` and a cleaner `--help` / `-h`.
- Unknown options now produce an error instead of being ignored.
- Multi-architecture build script: compiles for `amd64`, `arm64`, and `riscv64`.
- Auto-downloads musl.cc toolchains if missing (no manual setup needed).
- Deterministic builds with `SOURCE_DATE_EPOCH` and `ZERO_AR_DATE`.
- Smaller binary via `-Os` + garbage-collected sections (~90–110 KB).
- Each binary reports its platform (`amd64` / `arm64` / `riscv64`).

### Example compression impact (180-page technical manual)

| Mode | Text stream bytes |
|---|---|
| Uncompressed (v1.7.2 style) | 824,314 |
| Compressed, level 12 | 261,247 |
| **Saved** | **68.3%** |

### Example size impact of CJK subsetting (unchanged from v1.7.2)

- Full Noto Sans JP embedded: ~5 MB PDF
- Same document with subsetting: ~75 KB PDF

## Compression Quick Start

```sh
# Default: compression ON, zlib level 12
./txt2pdf input.txt output.pdf

# Custom level (1 = fastest, 12 = smallest)
./txt2pdf --level 6 input.txt output.pdf

# Disable compression (classic v1.7.2 output)
./txt2pdf --no-compress input.txt output.pdf
```

At the end of each run a summary line is printed:

```text
Compression: zlib level 12 (libdeflate) | streams 824314 -> 261247 bytes (68.3% saved)
```

## URL Links

External URLs in the text are automatically detected and converted to clickable links:

- `http://example.com`
- `https://github.com/johna124`
- `ftp://files.example.org`
- `www.example.com` (automatically prefixed with `https://`)

URLs are rendered in **blue with underline** (HTML-style) and are clickable in any PDF viewer. Trailing punctuation (`. , ; : ! ?`) is automatically stripped from the link.

Example:

```text
Also http://example.com and www.example.org are working.
```

All three URLs become clickable blue underlined links.

## CJK Quick Start

```sh
./txt2pdf --font NotoSansJP-Regular.ttf roadmap.txt roadmap.pdf
./txt2pdf --font NotoSansSC-Regular.ttf input.txt output.pdf
./txt2pdf --font NotoSansKR-Regular.ttf input.txt output.pdf
```

Important:

- The font must be an individual TrueType `.ttf` file.
- OpenType/CFF `.otf` files are not supported yet.
- TrueType collections `.ttc` are not supported yet.
- When `--font` is used, it overrides `--sans`, `--serif` and `--mono`.

## Building

```sh
./compile.sh
```

The script:

1. Builds libdeflate as a static library (uses its Makefile if present, otherwise compiles the `libdeflate/lib` sources directly). **No CMake needed.**
2. Verifies the library exports the required symbols.
3. Links `txt2pdf` as a fully static, stripped binary.

Size-oriented flags: `-Os`, `-ffunction-sections -fdata-sections`, `-Wl,--gc-sections`, `-fno-unwind-tables`, `-Wl,--build-id=none`, `-s`.

### Multi-Architecture Builds

```sh
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

- `x86_64-linux-musl-cross.tgz` (~100 MB)
- `aarch64-linux-musl-cross.tgz` (~100 MB)
- `riscv64-linux-musl-cross.tgz` (~100 MB)

Toolchains are cached in `.toolchains/` and reused.

### Deterministic Builds

Builds are reproducible with `SOURCE_DATE_EPOCH=1179146266` and `ZERO_AR_DATE=1`. Same input → same output hash every time.

### Binary Sizes (typical)

| Architecture | Size |
|---|---|
| amd64 | ~101 KB |
| arm64 | ~110 KB |
| riscv64 | ~90 KB 🏆 |

RISC-V is the smallest thanks to RVC (compressed 16-bit instructions).

## Why txt2pdf? (Comparison)

If you just need to generate a structured PDF from raw text, code, or logs, using massive office suites or proprietary software is like using a rocket launcher to swat a fly.

| | Adobe Acrobat Pro | LibreOffice / Pandoc | txt2pdf |
|---|---|---|---|
| Primary Interface | Heavy GUI | CLI / GUI (Java/Python) | Minimalist CLI |
| Dependencies | Massive proprietary ecosystem | Huge (hundreds of MBs / runtimes) | None at runtime (pure C11 + embedded libdeflate) |
| Binary Size | Gigabytes | 100 MB to 1 GB+ | ~90–110 KB static binary |
| Architecture Support | x86_64, ARM64 (limited) | x86_64, ARM64 | x86_64, ARM64, RISC-V (RV32/RV64) |
| Execution Speed | Slow startup | Moderate (heavy parsing overhead) | Blazing fast |
| Plain Text Outlines / Links | No (manual styling) | No (Markdown/LaTeX syntax) | Yes (auto-generated from raw text) |
| Stream Compression | Yes | Yes | Yes (zlib via libdeflate, level 12 default) |
| External URL Links | Yes | Yes | Yes (auto-detected, blue + underline) |
| Unicode CJK Embedding | Yes | Yes | Yes, with embedded TTF subset |
| Deterministic Builds | No | No | Yes (`SOURCE_DATE_EPOCH`) |
| Target Audience | Enterprise / end-users | General document automation | Systems hackers, minimalists, embedded systems |

## The Spartan Advantage

While Adobe Acrobat and LibreOffice are excellent for rich document design, they treat plain text as a raw, unstructured block. Generating table of contents, clickable navigation, or document outlines requires tedious manual formatting. Even Pandoc forces you to rewrite your files into Markdown or LaTeX syntax first.

`txt2pdf` cuts the bloat entirely. It natively parses your raw text stream, automatically detects document hierarchy, and embeds cross-linked internal navigation and PDF outlines out of the box, all while running inside an ultra-low-power RISC-V/ARM64 dev-board or bare-metal environment using minimal CPU and RAM.

With v1.7.2, `txt2pdf` gained subsetted TrueType embedding for real Unicode CJK text. With v1.8.0, it also squeezes every text stream through libdeflate at level 12 — cutting typical text-heavy PDFs by ~60–70% — and turns every `http://`/`https://`/`www.` URL into a clickable blue underlined link, without ever touching the already-compressed image data.

## How It Works (The Core Parser)

Unlike standard text editors that treat files as a flat sequence of characters, `txt2pdf` features a smart, single-pass streaming parser. It scans your raw text and automatically structures the output based on predictable plaintext patterns:

- **Automatic Outlines:** Identifies document hierarchies by analyzing line breaks and typography, injecting them into the native PDF interactive sidebar.
- **Smart Internal Links:** Any detected structural reference within the TOC block is converted into a clickable internal hyperlink.
- **Smart External Links:** URLs in the text are detected, underlined in blue, and converted to `/URI` annotations.
- **Stream Processing:** Reads and converts text efficiently without heavyweight document frameworks.
- **Compressed Streams:** Content streams, ToUnicode CMap and FontFile2 are written through libdeflate (zlib format) and declared with `/Filter /FlateDecode`; offsets are computed after compression, keeping the xref table valid.

## Hardware Footprint and Efficiency

Built for systems hackers and ultra-low-power environments, `txt2pdf` is designed to run where modern runtimes choke:

- **Memory Usage:** Very low RAM usage for normal text documents.
- **Storage Footprint:** ~90–110 KB static binary when stripped.
- **CPU Overhead:** Near-zero parsing overhead for text layout and PDF object generation. Level 12 compression favors size over speed; use `--level 1..6` on constrained devices if generation speed matters more than PDF size.

When CJK mode is enabled, memory usage depends mainly on the source TTF file and the number of glyphs used.

## License

- Source Code: GPLv3
- Documentation / Manual: GFDL v1.3
- libdeflate: MIT license, Copyright 2016 Eric Biggers
