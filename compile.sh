#!/bin/sh
# =============================================================================
# txt2pdf v1.8.0 - hybrid multi-architecture build script
# Usage: ./compile.sh [amd64|arm64|riscv64|all]
#        (no argument -> build for the host architecture)
#
# Features:
#   - Auto-downloads musl.cc toolchains if missing (arm64, riscv64, amd64)
#   - Deterministic builds (SOURCE_DATE_EPOCH, ZERO_AR_DATE)
#   - Per-architecture libdeflate builds under libdeflate/build-$ARCH/
#   - Static, stripped, size-optimised binaries
# =============================================================================
set -e

# ============================================================
# DETERMINISTIC BUILD SETTINGS
# ============================================================
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-1793319448}"
export ZERO_AR_DATE=1
echo "🔒 Deterministic build frozen at epoch: $SOURCE_DATE_EPOCH"
echo ""

# ============================================================
# COMPILATION FLAGS (size-optimised)
# ============================================================
SIZE_CFLAGS="-Os -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-ident"
SIZE_LDFLAGS="-static -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,noseparate-code -s"

# ============================================================
# HOST ARCHITECTURE DETECTION
# ============================================================
case "$(uname -m)" in
    x86_64)         HOST_ARCH=amd64 ;;
    aarch64|arm64)  HOST_ARCH=arm64 ;;
    riscv64)        HOST_ARCH=riscv64 ;;
    *)              HOST_ARCH=amd64 ;;
esac

CROSS_DIR="$(pwd)/.toolchains"
mkdir -p "$CROSS_DIR"

# ============================================================
# TOOLCHAIN SETUP (auto-download from musl.cc if missing)
# ============================================================
download_toolchain() {
    arch="$1"
    url="$2"
    tarball="$(basename "$url")"
    dirname="${tarball%.tgz}"

    if [ -d "$CROSS_DIR/$dirname" ]; then
        return 0
    fi

    echo "⚠️  Cross-compiler for $arch not found in PATH."
    echo "📥 Downloading ~100 MB of pure silicon from musl.cc..."
    mkdir -p "$CROSS_DIR"
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
        echo "❌ ERROR: neither wget nor curl available. Install one and retry."
        exit 1
    fi

    echo "📦 Extracting cross-compilation environment..."
    tar -xzf "$tarball"
    cd - >/dev/null
}

setup_toolchain() {
    arch="$1"
    case "$arch" in
        amd64)
            # Try host-native musl-gcc first, then cross toolchain
            if command -v musl-gcc >/dev/null 2>&1; then
                CC="musl-gcc"
                STRIP_BIN="strip"
                return 0
            fi
            download_toolchain "$arch" "https://musl.cc/x86_64-linux-musl-cross.tgz"
            CC="$CROSS_DIR/x86_64-linux-musl-cross/bin/x86_64-linux-musl-gcc"
            STRIP_BIN="$CROSS_DIR/x86_64-linux-musl-cross/bin/x86_64-linux-musl-strip"
            ;;
        arm64)
            if command -v aarch64-linux-musl-gcc >/dev/null 2>&1; then
                CC="aarch64-linux-musl-gcc"
                STRIP_BIN="aarch64-linux-musl-strip"
                return 0
            fi
            download_toolchain "$arch" "https://musl.cc/aarch64-linux-musl-cross.tgz"
            CC="$CROSS_DIR/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
            STRIP_BIN="$CROSS_DIR/aarch64-linux-musl-cross/bin/aarch64-linux-musl-strip"
            ;;
        riscv64)
            if command -v riscv64-linux-musl-gcc >/dev/null 2>&1; then
                CC="riscv64-linux-musl-gcc"
                STRIP_BIN="riscv64-linux-musl-strip"
                return 0
            fi
            download_toolchain "$arch" "https://musl.cc/riscv64-linux-musl-cross.tgz"
            CC="$CROSS_DIR/riscv64-linux-musl-cross/bin/riscv64-linux-musl-gcc"
            STRIP_BIN="$CROSS_DIR/riscv64-linux-musl-cross/bin/riscv64-linux-musl-strip"
            ;;
        *)
            echo "❌ Unknown architecture: $arch"
            exit 1
            ;;
    esac

    if [ ! -x "$CC" ]; then
        echo "❌ ERROR: $CC not found even after deploying the toolchain."
        exit 1
    fi

    # Export so sub-Makefiles and sub-processes inherit the right compiler
    export CC
    export PATH="$(dirname "$CC"):$PATH"
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
    arch="$1"; CC="$2"
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
    ar rcD "$BUILDDIR/libdeflate.a" $OBJ
}

check_lib() {
    lib="$1"; arch="$2"
    nm "$lib" | grep -q " T libdeflate_zlib_compress" || return 1
    case "$arch" in
        amd64) nm "$lib" | grep -q " [BD] libdeflate_x86_cpu_features" || return 1 ;;
        arm64) nm "$lib" | grep -q " [BD] libdeflate_arm_cpu_features" || return 1 ;;
        # riscv64: no CPU dispatch, no extra symbols
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

    build_libdeflate "$arch" "$CC"
    LIB="libdeflate/build-$arch/libdeflate.a"

    if ! check_lib "$LIB" "$arch"; then
        echo "❌ $LIB looks incomplete. Try: rm -rf libdeflate/build-$arch && retry."
        exit 1
    fi

    "$CC" -Wall -Wextra -std=c11 $SIZE_CFLAGS $SIZE_LDFLAGS \
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
ARCH="${1:-$HOST_ARCH}"

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
        for b in txt2pdf-amd64 txt2pdf-arm64 txt2pdf-riscv64; do
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
    *)
        echo "❌ Invalid argument: $ARCH"
        echo ""
        echo "Usage: $0 [amd64|arm64|riscv64|all]"
        echo ""
        echo "  amd64   → x86_64 (Intel/AMD)"
        echo "  arm64   → aarch64 (ARM 64-bit)"
        echo "  riscv64 → RISC-V 64-bit"
        echo "  all     → build all three architectures at once"
        echo "  (no argument → build for the host architecture)"
        exit 1
        ;;
esac
