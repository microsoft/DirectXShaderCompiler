#!/usr/bin/env python3
"""Setup Agility SDK binaries for DXC execution tests.

Automates downloading and installing the latest D3D12 Agility SDK binaries
into the TAEF directory of an hlsl.bin build tree, so exec tests can run
with the latest D3D12 runtime.

Usage:
    python setup_agility_sdk.py [hlsl_bin_dir] [options]

Examples:
    python setup_agility_sdk.py F:\\hlsl.bin
    python setup_agility_sdk.py F:\\hlsl.bin --sdk-type preview
    python setup_agility_sdk.py --overwrite
"""

import argparse
import glob
import logging
import os
import shutil
import sys
import tempfile
import zipfile

DEFAULT_HLSL_BIN = r"F:\hlsl.bin"
NETWORK_SHARE = r"\\GRFXSHARE\Sigma-GRFX\Users\amarp\IHVDrops"
ZIP_PATTERN = "D3D12_AgilitySDK_preview_*"
AGILITY_DLLS = ["D3D12Core.dll", "D3D12SDKLayers.dll"]

log = logging.getLogger("setup_agility_sdk")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Setup Agility SDK binaries for DXC execution tests.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  %(prog)s F:\\hlsl.bin\n"
            "  %(prog)s F:\\hlsl.bin --sdk-type preview\n"
            "  %(prog)s --overwrite\n"
        ),
    )
    parser.add_argument(
        "hlsl_bin_dir",
        nargs="?",
        default=None,
        help="Path to the hlsl.bin build directory (default: %(default)s).",
    )
    parser.add_argument(
        "--sdk-type",
        choices=["experimental", "preview"],
        default="experimental",
        help="Agility SDK flavor to install (default: experimental).",
    )
    parser.add_argument(
        "--arch",
        choices=["x64", "x86", "ARM64"],
        default="x64",
        help="Target architecture (default: x64).",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing Agility SDK binaries even if already present.",
    )
    return parser.parse_args()


def validate_hlsl_bin(hlsl_bin_dir):
    """Validate that the hlsl.bin directory exists."""
    if not os.path.isdir(hlsl_bin_dir):
        log.error("hlsl.bin directory does not exist: %s", hlsl_bin_dir)
        sys.exit(1)
    log.info("Using hlsl.bin directory: %s", hlsl_bin_dir)


def validate_taef_dir(hlsl_bin_dir, arch):
    """Validate that the TAEF/<arch> directory exists under hlsl.bin."""
    taef_dir = os.path.join(hlsl_bin_dir, "TAEF", arch)
    if not os.path.isdir(taef_dir):
        log.error(
            "TAEF directory not found: %s\n"
            "  Make sure you have run hctstart.cmd to initialize the build environment.",
            taef_dir,
        )
        sys.exit(1)
    log.info("Found TAEF directory: %s", taef_dir)
    return taef_dir


def check_existing_sdk(taef_dir):
    """Check if D3D12 Agility SDK DLLs already exist. Returns the D3D12 dir path."""
    d3d12_dir = os.path.join(taef_dir, "D3D12")
    if not os.path.isdir(d3d12_dir):
        log.info("D3D12 directory does not exist yet: %s", d3d12_dir)
        return d3d12_dir, False

    missing = [f for f in AGILITY_DLLS if not os.path.isfile(os.path.join(d3d12_dir, f))]
    if missing:
        log.info("Agility SDK incomplete, missing: %s", ", ".join(missing))
        return d3d12_dir, False

    log.info("Agility SDK binaries already present in: %s", d3d12_dir)
    return d3d12_dir, True


def access_network_share():
    """Verify network share is accessible."""
    log.info("Checking network share access: %s", NETWORK_SHARE)
    if not os.path.isdir(NETWORK_SHARE):
        log.error(
            "Cannot access network share: %s\n"
            "  Must have corpnet or VPN access.",
            NETWORK_SHARE,
        )
        sys.exit(1)
    log.info("Network share is accessible.")


def find_newest_zip():
    """Find the newest Agility SDK zip on the network share."""
    pattern = os.path.join(NETWORK_SHARE, ZIP_PATTERN + ".zip")
    zips = sorted(glob.glob(pattern))
    if not zips:
        log.error("No Agility SDK zips found matching: %s", pattern)
        sys.exit(1)
    newest = zips[-1]
    log.info("Found %d SDK zip(s). Using newest: %s", len(zips), os.path.basename(newest))
    return newest


def extract_and_copy(zip_path, sdk_type, arch, d3d12_dir):
    """Extract the SDK zip to a temp directory and copy binaries."""
    tmp_dir = tempfile.mkdtemp(prefix="agility_sdk_")
    try:
        log.info("Extracting %s to temp directory...", os.path.basename(zip_path))
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(tmp_dir)

        # The zip extracts into a top-level directory named like the zip (without .zip).
        # Find the actual extraction root.
        top_dirs = [
            d for d in os.listdir(tmp_dir) if os.path.isdir(os.path.join(tmp_dir, d))
        ]

        # Source path: <extract_root>/<sdk_type>/<arch>/sdkbin/
        # Try with and without a top-level wrapper directory.
        candidates = [os.path.join(tmp_dir, sdk_type, arch, "sdkbin")]
        for td in top_dirs:
            candidates.insert(0, os.path.join(tmp_dir, td, sdk_type, arch, "sdkbin"))

        src_dir = None
        for c in candidates:
            if os.path.isdir(c):
                src_dir = c
                break

        if src_dir is None:
            log.error(
                "Could not find SDK binaries in extracted zip.\n"
                "  Expected path: <zip_root>/%s/%s/sdkbin/\n"
                "  Searched:\n    %s",
                sdk_type,
                arch,
                "\n    ".join(candidates),
            )
            sys.exit(1)

        log.info("Found SDK binaries at: %s", src_dir)

        # Create destination D3D12 directory if needed.
        os.makedirs(d3d12_dir, exist_ok=True)

        # Copy only the Agility SDK DLLs and their PDBs.
        target_stems = {os.path.splitext(f)[0].lower() for f in AGILITY_DLLS}
        copied = []
        for fname in os.listdir(src_dir):
            stem = os.path.splitext(fname)[0].lower()
            ext = os.path.splitext(fname)[1].lower()
            if stem in target_stems and ext in (".dll", ".pdb"):
                src = os.path.join(src_dir, fname)
                dst = os.path.join(d3d12_dir, fname)
                shutil.copy2(src, dst)
                copied.append(fname)
                log.info("  Copied: %s", fname)

        if not copied:
            log.warning("No DLL/PDB files found in %s", src_dir)
        else:
            log.info("Copied %d file(s) to %s", len(copied), d3d12_dir)

    finally:
        log.info("Cleaning up temp directory: %s", tmp_dir)
        shutil.rmtree(tmp_dir, ignore_errors=True)


def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(levelname)-7s %(message)s",
    )

    args = parse_args()

    # Resolve hlsl.bin directory.
    if args.hlsl_bin_dir is None:
        log.warning(
            "No hlsl.bin path provided, defaulting to %s", DEFAULT_HLSL_BIN
        )
        hlsl_bin_dir = DEFAULT_HLSL_BIN
    else:
        hlsl_bin_dir = args.hlsl_bin_dir

    hlsl_bin_dir = os.path.abspath(hlsl_bin_dir)

    # Step 1: Validate hlsl.bin.
    validate_hlsl_bin(hlsl_bin_dir)

    # Step 2: Validate TAEF directory.
    taef_dir = validate_taef_dir(hlsl_bin_dir, args.arch)

    # Step 3: Check for existing Agility SDK.
    d3d12_dir, already_present = check_existing_sdk(taef_dir)
    if already_present and not args.overwrite:
        log.info("Nothing to do. Use --overwrite to force update.")
        return
    if already_present and args.overwrite:
        log.info("--overwrite specified, will replace existing binaries.")

    # Step 4: Access network share.
    access_network_share()

    # Step 5: Find newest SDK zip.
    zip_path = find_newest_zip()

    # Step 6-7: Extract and copy.
    extract_and_copy(zip_path, args.sdk_type, args.arch, d3d12_dir)

    log.info("Agility SDK setup complete.")


if __name__ == "__main__":
    main()
