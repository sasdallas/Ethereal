#!/usr/bin/python3

# get_userspace_lib_info.py - Returns a variety of information about a userspace library

import sys
import os

if len(sys.argv) < 3:
    print("Usage: get_userspace_lib_info.py <working dir> <request>")
    print("Available requests: NAME")
    sys.exit(1)

WORKING_DIR = sys.argv[1]
REQUEST = sys.argv[2]


LIB_FILE = os.path.join(WORKING_DIR, "lib.conf")
if not os.path.exists(LIB_FILE):
    raise FileNotFoundError("lib.conf not found in " + WORKING_DIR)

f = open(LIB_FILE, "r")
lines = f.read().splitlines()
f.close()

# Check request
if REQUEST == "NAME":
    # Name of the app
    for line in lines:
        if line.startswith("LIB_NAME = "):
            print(line.replace("\"", "").replace("LIB_NAME = ", ""))
            sys.exit(0)

    raise ValueError("LIB_NAME not found in lib.conf")
else:
    raise ValueError("Invalid REQUEST: " + REQUEST)


