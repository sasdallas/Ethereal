#!/usr/bin/python3

import tarfile 
import sys

if len(sys.argv) < 3:
    print("Usage: mkinitrd.py <tar file> <directory>")
    sys.exit(0)

file = sys.argv[1]
dir = sys.argv[2]

mode_overrides = {
    "tmp": 0o775,       # /tmp: drwxrwxr-x
    "var": 0o775,       # /var: drwxrwxr-x
}

def ramdisk_filter(tarinfo):
    tarinfo.uid = 0
    tarinfo.gid = 0

    if tarinfo.name in mode_overrides:
        tarinfo.mode = mode_overrides[tarinfo.name]

    return tarinfo

with tarfile.open(file, "w", format=tarfile.USTAR_FORMAT) as ramdisk:
    ramdisk.add(dir, arcname="/", filter=ramdisk_filter)