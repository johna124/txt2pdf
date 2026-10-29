# TXT2PDF (v1.7.2) - THE SPARTAN PDF GENERATOR

`txt2pdf` is a zero-dependency, command-line utility written in pure C11 that converts plain text files into structured PDF 1.4 documents.

## Core Features

- **Pure C11:** Zero dependencies, no C++ runtime, no external libraries.
- **Tiny & Static:** Can be built as a self-contained static binary using musl libc.
- **Rich PDF Output:** Generates valid PDF 1.4 documents with outlines and internal links.
- **Base14 Fonts:** Built-in support for Courier, Helvetica, and Times-Roman.
- **Native Image Passthrough:** Supports JPEG and PNG embedding.
- **Unicode CJK Fonts:** Optional embedded TrueType font support via `--font FILE.ttf`.
- **TrueType Subsetting:** Embeds only the glyphs used by the document.
- **Architecture Agnostic:** Cross-compiles cleanly to RISC-V, x86_64, and ARM with zero code changes.

## What Is New In v1.7.2

Version 1.7.2 adds optional Unicode font support for Japanese, Chinese, Korean and other scripts covered by the selected TrueType font.

Main additions:

- Added `--font FILE.ttf` mode.
- Added embedded TrueType font support.
- Added Unicode CJK output using PDF `Type0` / `CIDFontType2`.
- Added `/Identity-H` encoding.
- Added `/ToUnicode` maps for copy, paste and search.
- Added real glyph metrics from the TTF for line wrapping.
- Added UTF-16BE bookmark titles for CJK outlines.
- Added automatic TrueType subsetting.
- Kept the classic Base14 mode unchanged when `--font` is not used.

Example size impact:

- Full Noto Sans JP embedded: ~5 MB PDF
- Same document with v1.7.2 subsetting: ~75 KB PDF

## CJK Quick Start

Examples:

    ./txt2pdf --font NotoSansJP-Regular.ttf roadmap.txt roadmap.pdf
    ./txt2pdf --font NotoSansSC-Regular.ttf input.txt output.pdf
    ./txt2pdf --font NotoSansKR-Regular.ttf input.txt output.pdf

Important:

- The font must be an individual TrueType `.ttf` file.
- OpenType/CFF `.otf` files are not supported yet.
- TrueType collections `.ttc` are not supported yet.
- When `--font` is used, it overrides `--sans`, `--serif` and `--mono`.

## Why txt2pdf? (Comparison)

If you just need to generate a structured PDF from raw text, code, or logs,
using massive office suites or proprietary software is like using a rocket
launcher to swat a fly.

- Primary Interface
  - Adobe Acrobat Pro: Heavy GUI
  - LibreOffice / Pandoc: CLI / GUI (requires Java/Python)
  - txt2pdf: Minimalist CLI

- Dependencies
  - Adobe Acrobat Pro: Massive proprietary ecosystem
  - LibreOffice / Pandoc: Huge (hundreds of MBs / runtimes)
  - txt2pdf: Zero (Pure C11)

- Binary Size
  - Adobe Acrobat Pro: Gigabytes
  - LibreOffice / Pandoc: 100 MB to 1 GB+
  - txt2pdf: Small static binary

- Architecture Support
  - Adobe Acrobat Pro: x86_64, ARM64 (Limited)
  - LibreOffice / Pandoc: x86_64, ARM64
  - txt2pdf: x86_64, ARM64, RISC-V (RV32/RV64)

- Execution Speed
  - Adobe Acrobat Pro: Slow startup
  - LibreOffice / Pandoc: Moderate (heavy parsing overhead)
  - txt2pdf: Blazing Fast

- Plain Text Outlines / Links
  - Adobe Acrobat Pro: No (Requires manual styling)
  - LibreOffice / Pandoc: No (Requires Markdown/LaTeX syntax)
  - txt2pdf: Yes (Auto-generates native PDF outlines and internal links directly from raw text)

- Unicode CJK Embedding
  - Adobe Acrobat Pro: Yes
  - LibreOffice / Pandoc: Yes
  - txt2pdf: Yes, with embedded TTF subset

- Target Audience
  - Adobe Acrobat Pro: Enterprise / End-users
  - LibreOffice / Pandoc: General document automation
  - txt2pdf: Systems hackers, minimalists, embedded systems

## The Spartan Advantage

While Adobe Acrobat and LibreOffice are excellent for rich document design,
they treat plain text as a raw, unstructured block. Generating table of
contents, clickable navigation, or document outlines requires tedious manual
formatting. Even Pandoc forces you to rewrite your files into Markdown or
LaTeX syntax first.

`txt2pdf` cuts the bloat entirely. It natively parses your raw text stream,
automatically detects document hierarchy, and embeds cross-linked internal
navigation and PDF outlines out of the box, all while running inside an
ultra-low-power RISC-V/ARM64 dev-board or bare-metal environment using
minimal CPU and RAM.

With v1.7.2, `txt2pdf` can also embed a subsetted TrueType font, allowing
real Unicode CJK text without pulling in huge external libraries.

## How It Works (The Core Parser)

Unlike standard text editors that treat files as a flat sequence of
characters, `txt2pdf` features a smart, single-pass streaming parser. It
scans your raw text and automatically structures the output based on
predictable plaintext patterns:

- Automatic Outlines: It identifies document hierarchies by analyzing line
  breaks and typography, automatically injecting them into the native PDF
  interactive sidebar.
- Smart Internal Links: Any detected structural reference within the TOC
  block is converted into a clickable internal hyperlink.
- Stream Processing: It reads and converts text efficiently without requiring
  heavyweight document frameworks.

## Hardware Footprint and Efficiency

Built for systems hackers and ultra-low-power environments, `txt2pdf` is
designed to run where modern runtimes choke:

- Memory Usage: Very low RAM usage for normal text documents.
- Storage Footprint: Small static binary when stripped.
- CPU Overhead: Near-zero parsing overhead for text layout and PDF object
  generation.

When CJK mode is enabled, memory usage depends mainly on the source TTF
file and the number of glyphs used.

## License

- Source Code: GPLv3
- Documentation / Manual: GFDL v1.3
