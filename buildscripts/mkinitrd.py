#!/usr/bin/python3

import tarfile 
import sys
import os

if len(sys.argv) < 3:
    print("Usage: mkinitrd.py <tar file> <directory>")
    sys.exit(0)

file = sys.argv[1]
dir = sys.argv[2]

try:
    os.unlink(file)
except FileNotFoundError:
    pass

mode_overrides = {
    "tmp": 0o775,       # /tmp: drwxrwxr-x
    "var": 0o775,       # /var: drwxrwxr-x
}

def ramdisk_filter(tarinfo):
    print("Packaging file: " + tarinfo.name)
    tarinfo.uid = 0
    tarinfo.gid = 0

    if tarinfo.name in mode_overrides:
        tarinfo.mode = mode_overrides[tarinfo.name]

    return tarinfo

with tarfile.open(file, "w:gz", format=tarfile.USTAR_FORMAT) as ramdisk:
    ramdisk.add(dir, arcname="/", filter=ramdisk_filter)