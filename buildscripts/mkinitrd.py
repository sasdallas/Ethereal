#!/usr/bin/python3
# ethereal's super advanced initrd generator

import argparse
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("archive", type=Path)
parser.add_argument("directory", type=Path)
args = parser.parse_args()

archive = args.archive.parent.resolve() / args.archive.name
directory = args.directory.resolve()
archive.parent.mkdir(parents=True, exist_ok=True)

mode_overrides = {
    "tmp": 0o775,
    "var": 0o775,
    "usr/bin/sudo": 0o4755,
}

excluded = {"boot/initrd.tar.img", "boot/initrd_tiny.tar.img"}
if archive.is_relative_to(directory):
    excluded.add(archive.relative_to(directory).as_posix())

with tempfile.NamedTemporaryFile(prefix=".initrd.", dir=archive.parent, delete=False) as temp:
    temporary = Path(temp.name)

def ramdisk_filter(member):
    if member.name.lstrip("/") in excluded or Path(member.name).name.startswith(".initrd."):
        return None
    member.uid = member.gid = 0
    if member.name in mode_overrides:
        member.mode = mode_overrides[member.name]
    return member

# gzip was faster than using tarfile.open's compression level, for some reason
try:
    with temporary.open("wb") as output:
        with subprocess.Popen(["gzip", "-6", "-n"], stdin=subprocess.PIPE, stdout=output) as gzip:
            with gzip.stdin:
                with tarfile.open(fileobj=gzip.stdin, mode="w|", format=tarfile.USTAR_FORMAT) as ramdisk:
                    ramdisk.add(directory, arcname="/", filter=ramdisk_filter)
            if gzip.wait():
                raise RuntimeError("gzip failed while packing the initrd")

    temporary.chmod(0o644)
    os.replace(temporary, archive)
finally:
    temporary.unlink(missing_ok=True)
