#!/bin/sh
# =============================================================================
# txt2pdf v1.9.0 - hybrid multi-architecture build script
# Usage: ./compile.sh [amd64|arm64|riscv64|all]
#        (no argument -> show options and exit)
#
# Features:
#   - Auto-downloads musl.cc toolchains if missing (arm64, riscv64, amd64)
#   - Deterministic builds (SOURCE_DATE_EPOCH, ZERO_AR_DATE)
#   - Embeds build epoch into txt2pdf -V
#   - Per-architecture libdeflate builds under libdeflate/build-$ARCH/
#   - Static, stripped, size-optimised binaries
#   - Creates ./txt2pdf when building for the host architecture
# =============================================================================
set -e

# ============================================================
# BASIC SANITY CHECKS
# ============================================================
if [ ! -f txt2pdf.c ]; then
    echo "❌ ERROR: txt2pdf.c not found in current directory." >&2
    exit 1
fi

if [ ! -d libdeflate/lib ]; then
    echo "❌ ERROR: libdeflate/lib not found. Download libdeflate sources first." >&2
    exit 1
fi

# ============================================================
# HOST ARCHITECTURE DETECTION
# ============================================================
case "$(uname -m)" in
    x86_64)         HOST_ARCH=amd64 ;;
    aarch64|arm64)  HOST_ARCH=arm64 ;;
    riscv64)        HOST_ARCH=riscv64 ;;
    *)              HOST_ARCH=amd64 ;;
esac

# ============================================================
# SHOW USAGE AND EXIT (no menu, no prompts)
# ============================================================
show_usage() {
    echo "🔧 txt2pdf v1.9.0 - Build System"
    echo ""
    echo "Usage: $0 [target]"
    echo ""
    echo "Available targets:"
    echo "  ⚙️  amd64    x86_64 (Intel/AMD)"
    echo "  ⚙️  arm64    aarch64 (Raspberry Pi, Apple Silicon, Graviton)"
    echo "  ⚙️  riscv64  RISC-V 64-bit (VisionFive, Milk-V, Star64)"
    echo "  🚀 all      Build all three architectures"
    echo ""
    echo "🏠 Detected host architecture: $HOST_ARCH"
    echo ""
    exit 0
}

# ============================================================
# NO ARGUMENTS: SHOW OPTIONS AND EXIT
# ============================================================
if [ $# -eq 0 ]; then
    show_usage
fi

ARCH="$1"

# ============================================================
# INVALID ARGUMENT: SHOW OPTIONS AND EXIT
# ============================================================
case "$ARCH" in
    amd64|arm64|riscv64|all) ;;
    *)
        echo "❌ Invalid argument: $ARCH" >&2
        echo "" >&2
        show_usage
        ;;
esac

# ============================================================
# DETERMINISTIC BUILD SETTINGS
# ============================================================
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-1793371215}"
export ZERO_AR_DATE=1

echo "🔒 Deterministic build frozen at epoch: $SOURCE_DATE_EPOCH"
echo ""

# ============================================================
# COMPILATION FLAGS (size-optimised)
# ============================================================
SIZE_CFLAGS="-Os -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-ident"
SIZE_LDFLAGS="-static -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,noseparate-code -s"

# Build-time epoch, visible from txt2pdf -V if the C source uses it.
EXTRA_CFLAGS="-DTXT2PDF_BUILD_EPOCH=${SOURCE_DATE_EPOCH}"

CROSS_DIR="$(pwd)/.toolchains"
mkdir -p "$CROSS_DIR"


# ============================================================
# TOOLCHAIN SETUP (auto-download from musl.cc if missing)
# ============================================================
download_toolchain() {
    arch="$1"
    url="$2"
    tarball="$(basename "$url")"
    toolchain_dir="${tarball%.tgz}"

    if [ -d "$CROSS_DIR/$toolchain_dir" ]; then
        return 0
    fi

    echo "⚠️  Cross-compiler for $arch not found in PATH."
    echo "📥 Downloading ~100 MB of pure silicon from musl.cc..."
    mkdir -p "$CROSS_DIR"

    (
        cd "$CROSS_DIR"

        if command -v wget >/dev/null 2>&1; then
            if [ ! -f "$tarball" ]; then
                wget -c --progress=bar:force "$url"
            fi
        elif command -v curl >/dev/null 2>&1; then
            if [ ! -f "$tarball" ]; then
                curl -fL --progress-bar -o "$tarball" "$url"
            fi
        else
            echo "❌ ERROR: neither wget nor curl available. Install one and retry." >&2
            exit 1
        fi

        echo "📦 Extracting cross-compilation environment..."
        tar -xzf "$tarball"
    )
}

setup_toolchain() {
    arch="$1"

    CC=""
    STRIP_BIN="strip"
    AR_BIN="ar"
    NM_BIN="nm"

    case "$arch" in
        amd64)
            # Only use generic host musl-gcc when we are actually on amd64.
            if [ "$arch" = "$HOST_ARCH" ] && command -v musl-gcc >/dev/null 2>&1; then
                CC="musl-gcc"
                STRIP_BIN="strip"
                AR_BIN="ar"
                NM_BIN="nm"
                return 0
            fi

            download_toolchain "$arch" "https://musl.cc/x86_64-linux-musl-cross.tgz"
            CROSS_BIN="$CROSS_DIR/x86_64-linux-musl-cross/bin"
            CC="$CROSS_BIN/x86_64-linux-musl-gcc"
            STRIP_BIN="$CROSS_BIN/x86_64-linux-musl-strip"
            AR_BIN="$CROSS_BIN/x86_64-linux-musl-ar"
            NM_BIN="$CROSS_BIN/x86_64-linux-musl-nm"
            ;;

        arm64)
            if command -v aarch64-linux-musl-gcc >/dev/null 2>&1; then
                CC="aarch64-linux-musl-gcc"
                STRIP_BIN="aarch64-linux-musl-strip"
                AR_BIN="aarch64-linux-musl-ar"
                NM_BIN="aarch64-linux-musl-nm"
                return 0
            fi

            download_toolchain "$arch" "https://musl.cc/aarch64-linux-musl-cross.tgz"
            CROSS_BIN="$CROSS_DIR/aarch64-linux-musl-cross/bin"
            CC="$CROSS_BIN/aarch64-linux-musl-gcc"
            STRIP_BIN="$CROSS_BIN/aarch64-linux-musl-strip"
            AR_BIN="$CROSS_BIN/aarch64-linux-musl-ar"
            NM_BIN="$CROSS_BIN/aarch64-linux-musl-nm"
            ;;

        riscv64)
            if command -v riscv64-linux-musl-gcc >/dev/null 2>&1; then
                CC="riscv64-linux-musl-gcc"
                STRIP_BIN="riscv64-linux-musl-strip"
                AR_BIN="riscv64-linux-musl-ar"
                NM_BIN="riscv64-linux-musl-nm"
                return 0
            fi

            download_toolchain "$arch" "https://musl.cc/riscv64-linux-musl-cross.tgz"
            CROSS_BIN="$CROSS_DIR/riscv64-linux-musl-cross/bin"
            CC="$CROSS_BIN/riscv64-linux-musl-gcc"
            STRIP_BIN="$CROSS_BIN/riscv64-linux-musl-strip"
            AR_BIN="$CROSS_BIN/riscv64-linux-musl-ar"
            NM_BIN="$CROSS_BIN/riscv64-linux-musl-nm"
            ;;

        *)
            echo "❌ Unknown architecture: $arch" >&2
            exit 1
            ;;
    esac

    if [ ! -x "$CC" ]; then
        echo "❌ ERROR: $CC not found even after deploying the toolchain." >&2
        exit 1
    fi

    # Fallback to host binutils if target-specific ones are missing.
    [ -x "$STRIP_BIN" ] || STRIP_BIN="strip"
    [ -x "$AR_BIN" ] || AR_BIN="ar"
    [ -x "$NM_BIN" ] || NM_BIN="nm"

    case "$CC" in
        */*) export PATH="$(dirname "$CC"):$PATH" ;;
    esac
}

describe_arch() {
    case "$1" in
        amd64)   echo "⚙️  Target: x86_64  (Intel / AMD desktop & servers)" ;;
        arm64)   echo "⚙️  Target: aarch64 (Raspberry Pi 4/5, Apple Silicon, Graviton, Pinebook)" ;;
        riscv64) echo "⚙️  Target: RISC-V 64-bit (VisionFive, Milk-V, Star64, Sipeed LM4A)" ;;
    esac
}

# ============================================================
# LIBDEFLATE PER-ARCHITECTURE BUILD
# ============================================================
build_libdeflate() {
    arch="$1"
    BUILDDIR="libdeflate/build-$arch"

    if [ -f "$BUILDDIR/libdeflate.a" ]; then
        echo ">> [$arch] libdeflate already built, reusing."
        return 0
    fi

    echo ">> [$arch] Building libdeflate with $CC ..."

    SRC=$(find libdeflate/lib -name '*.c' | sort)

    for f in $SRC; do
        rel=${f#libdeflate/lib/}
        obj="$BUILDDIR/${rel%.c}.o"
        mkdir -p "$(dirname "$obj")"
        "$CC" $SIZE_CFLAGS -Ilibdeflate -Ilibdeflate/lib -Ilibdeflate/common \
            -c "$f" -o "$obj"
    done

    OBJ=$(find "$BUILDDIR" -name '*.o' | sort)

    # `D` flag + ZERO_AR_DATE=1 → deterministic archive (timestamps = 0)
    "$AR_BIN" rcD "$BUILDDIR/libdeflate.a" $OBJ
}

check_lib() {
    lib="$1"
    arch="$2"

    "$NM_BIN" "$lib" | grep -q " T libdeflate_zlib_compress" || return 1

    case "$arch" in
        amd64)
            "$NM_BIN" "$lib" | grep -q " [BD] libdeflate_x86_cpu_features" || return 1
            ;;
        arm64)
            "$NM_BIN" "$lib" | grep -q " [BD] libdeflate_arm_cpu_features" || return 1
            ;;
        riscv64)
            # riscv64: no CPU dispatch, no extra symbols
            ;;
    esac

    return 0
}

# ============================================================
# BUILD TXT2PDF FOR ONE ARCHITECTURE
# ============================================================
build_one() {
    arch="$1"

    echo ""
    describe_arch "$arch"
    setup_toolchain "$arch"

    OUT="txt2pdf-$arch"
    echo ">> [$arch] Compiler: $CC"
    echo ">> [$arch] Build epoch: $SOURCE_DATE_EPOCH"

    build_libdeflate "$arch"
    LIB="libdeflate/build-$arch/libdeflate.a"

    if ! check_lib "$LIB" "$arch"; then
        echo "❌ $LIB looks incomplete. Try: rm -rf libdeflate/build-$arch && retry." >&2
        exit 1
    fi

    "$CC" -Wall -Wextra -std=c11 \
        $SIZE_CFLAGS $EXTRA_CFLAGS $SIZE_LDFLAGS \
        -Ilibdeflate -Ilibdeflate/include \
        -o "$OUT" txt2pdf.c "$LIB"

    # Use the architecture-specific strip if available, fallback to host strip
    if [ -n "$STRIP_BIN" ] && command -v "$STRIP_BIN" >/dev/null 2>&1; then
        "$STRIP_BIN" "$OUT"
    else
        strip "$OUT"
    fi

    SIZE=$(wc -c < "$OUT" | tr -d ' ')
    echo "✅ $OUT : $SIZE bytes ($((SIZE / 1024)) KB)"

    file "$OUT" || true
}

# ============================================================
# MAIN
# ============================================================

case "$ARCH" in
    all)
        echo "🚀 Building for ALL architectures..."
        echo ""

        for t in amd64 arm64 riscv64; do
            build_one "$t" || exit 1
        done

        echo ""
        echo "========================================================================="
        echo "📦 Final binaries:"

        for b in txt2pdf-amd64 txt2pdf-arm64 txt2pdf-riscv64 txt2pdf; do
            if [ -f "$b" ]; then
                SIZE_BYTES=$(wc -c < "$b" | tr -d ' ')
                SIZE_KB=$((SIZE_BYTES / 1024))
                echo "  $b : $SIZE_BYTES bytes ($SIZE_KB KB)"
            fi
        done

        echo ""
        echo "🧬 Binary fingerprints:"

        for b in txt2pdf-amd64 txt2pdf-arm64 txt2pdf-riscv64; do
            if [ -f "$b" ]; then
                printf "  %-22s " "$b"
                file "$b" | sed 's/^[^:]*: //'
            fi
        done

        echo "========================================================================="
        ;;

    amd64|arm64|riscv64)
        build_one "$ARCH"
        ;;
esac
