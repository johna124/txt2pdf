# TXT2PDF (v1.7) - THE SPARTAN PDF GENERATOR

`txt2pdf` is a zero-dependency, command-line utility written in pure C11 that converts plain text files into structured PDF 1.4 documents.

## Core Features

- **Pure C11:** Zero dependencies, no C++ runtime, and no external libraries.
- **Tiny & Static:** Can be built as a self-contained static binary using musl libc.
- **Rich PDF Output:** Generates valid PDF 1.4 documents with outlines and internal links.
- **Base14 Fonts:** Built-in support for Courier, Helvetica, and Times-Roman.
- **Native Image Passthrough:** Supports JPEG and PNG embedding.
- **Architecture Agnostic:** Cross-compiles beautifully to RISC-V, x86_64, and ARM with zero code changes.

## Why `txt2pdf`? (Comparison)

If you just need to generate a structured PDF from raw text, code, or logs, using massive office suites or proprietary software is like using a rocket launcher to swat a fly. 

Here is how `txt2pdf` stacks up against heavy alternatives:


| Feature / Metric | Adobe Acrobat Pro | LibreOffice / Pandoc | `txt2pdf` (The Spartan Way) |
| :--- | :--- | :--- | :--- |
| **Primary Interface** | Heavy GUI | CLI / GUI (requires Java/Python) | Minimalist CLI |
| **Dependencies** | Massive proprietary ecosystem | Huge (hundreds of MBs / runtimes) | **Zero** (Pure C11) |
| **Binary Size** | Gigabytes | 100MB+ to 1GB+ | **~58 KB** (Statically linked) |
| **Architecture Support** | x86_64, ARM64 (Limited) | x86_64, ARM64 | **x86_64, ARM64, RISC-V (RV32/RV64)** |
| **Execution Speed** | Slow startup | Moderate (heavy parsing overhead) | **Blazing Fast** (Near-instantaneous) |
| **Plain Text Outlines / Links** | ❌ No (Requires manual styling) | ❌ No (Requires Markdown/LaTeX syntax) | **✅ Yes (Auto-generates native PDF outlines and internal links directly from raw text)** |
| **Target Audience** | Enterprise / End-users | General document automation | Systems hackers, minimalists, embedded systems |

### The Spartan Advantage
While **Adobe Acrobat** and **LibreOffice** are excellent for rich document design, they treat plain text as a raw, unstructured block. Generating table of contents, clickable navigation, or document outlines requires tedious manual formatting. Even **Pandoc** forces you to rewrite your files into Markdown or LaTeX syntax first.

`txt2pdf` cuts the bloat entirely. It natively parses your raw text stream, automatically detects document hierarchy, and embeds cross-linked internal navigation and PDF outlines out of the box—all while running inside an ultra-low-power **RISC-V/ARM64** dev-board or bare-metal environment using minimal CPU and RAM.


## How It Works (The Core Parser)

Unlike standard text editors that treat files as a flat sequence of characters, `txt2pdf` features a smart, single-pass streaming parser. It scans your raw text and automatically structures the output based on predictable plaintext patterns:

- **Automatic Outlines:** It identifies document hierarchies (like chapter titles, logs headers, or section dividers) by analyzing line breaks and typography, automatically injecting them into the native PDF interactive sidebar.
- **Smart Internal Links:** Any detected structural reference or cross-reference within the text stream is converted into a clickable internal hyperlink, allowing seamless navigation without modifying the source file.
- **Stream Processing:** It reads and converts chunks on the fly. This means it can process infinitely large text files or streaming logs via standard input (`stdin`) without loading the entire document into memory.

## Hardware Footprint & Efficiency

Built for systems hackers and ultra-low-power environments, `txt2pdf` is designed to run where modern runtimes choke:

- **Memory Usage:** Consumes less than **1-2 MB of RAM** during execution (depending on the internal buffer size), making it perfect for embedded Linux setups and microcontrollers.
- **Storage Footprint:** The binary compiles down to roughly **~58 KB** when statically linked and stripped. It easily fits into tiny boot partitions or ROMs.
- **CPU Overhead:** Near-zero parsing overhead. It converts thousands of lines of text per millisecond, dropping the CPU back to idle almost instantly.


## License

- **Source Code:** GPLv3
- **Documentation / Manual:** GFDL v1.3
