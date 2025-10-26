#!/usr/bin/python3

# lib.py: Userspace library information

import argparse
import sys
import os
import subprocess

class Library():
    def __init__(self, name, desc, version, deps, deps_private, cflags, shared):
        self.name = name
        self.desc = desc
        self.version = version
        self.deps = deps
        self.deps_private = deps_private
        self.cflags = cflags
        self.shared = shared
        self.desc = desc

def preprocess(line, no_spaces=False, quotes=False):
    v = (line.replace(" = ", "=").split("=")[1])
    if no_spaces and " " in v:
        raise Exception("Spaces not allowed for this element: " + line)
    if not quotes and "\"" in v:
        raise Exception("Quotes not allowed for this element: " + line)
    return v

def look_or_substitute(lines, key, sub, no_spaces=False, quotes=False):
    for line in lines:
        if line.startswith(key):
            return preprocess(line, no_spaces=no_spaces, quotes=quotes)
        
    if sub is None:
        raise Exception("{} cannot be empty".format(key))
    return sub

def create_library_from_info_file(filename):
    f = open(filename, "r")
    lines = [line.strip() for line in f.readlines()]
    f.close()

    n = look_or_substitute(lines, "NAME", None, no_spaces=True)

    if not n.startswith("lib"):
        n = "lib" + n

    v = look_or_substitute(lines, "VERSION", "1.0.0", no_spaces=True)
    desc = look_or_substitute(lines, "DESCRIPTION", "", no_spaces=False, quotes=True)
    d = look_or_substitute(lines, "DEPENDS_ON", "", no_spaces=False)

    assert len(v.split(".")) == 3, "VERSION must have the format X.X.X"
    assert len([l for l in v.split(".") if l.isdigit()]) == 3, "All components of VERSION must be number"

    if d != "":
        d = d.replace(" ", "")
        d = d.split(",")
        for i in d:
            if i.startswith("lib"):
                i = i[3:]
        

    # DP is just used for PC generation
    
    dp = look_or_substitute(lines, "DEPENDS_ON_PRIV", "", no_spaces=False)

    if dp != "":
        dp = d.replace(" ", "")
        dp = d.split(",")
        for i in dp:
            if i.startswith("lib"):
                i = i[3:]
        

    cflags = look_or_substitute(lines, "CFLAGS", "", no_spaces=False)

    shared = len([line for line in lines if line.startswith("NO_SHARED")]) == 0

    return Library(n, desc, v, d, dp, cflags, shared)

def collect_cflags(lib: Library):
    # collect library cflags from each of its dependecies
    cflags = ""
    r = ""
    for i in lib.deps:
       r = r + " " + i
    if r == "":
        return ""
    
    
    o = subprocess.run(["pkg-config", "--cflags", r])
    if o.returncode != 0:
        raise Exception("returncode != 0")
    

    if o.stdout is None:
        return ""    

    cflags = cflags + " " + o.stdout.decode("utf-8")
    return cflags




if __name__ == "__main__":
    parser = argparse.ArgumentParser("lib")
    parser.add_argument("filename")
    parser.add_argument("--build_symlinks", action="store_true")
    parser.add_argument("--cflags", action="store_true")
    parser.add_argument("--name", action="store_true")
    parser.add_argument("--version", action="store_true")
    parser.add_argument("--shared", action="store_true")
    parser.add_argument("--generate-build-rules", action="store_true")
    ap = parser.parse_args()

    # create library info
    lib = create_library_from_info_file(ap.filename)

    if ap.build_symlinks:
        if not lib.shared:
            sys.exit(0) # only symlink shared libraries
        path = os.environ["SYSROOT"] + os.environ["PREFIX"] + "/lib/" + lib.name + ".so"
        print("Building symlinks to " + path)
    

        a = lib.version.split(".")
        try:
            os.symlink(path, path + "." + lib.version)
            os.symlink(path, path + "." + a[0])
        except Exception:
            pass

    if ap.generate_build_rules:
        f = open("generated_build_rules.mk", "w+")
        l = ""


        l = l + "LIB_PREFIX := {}\n".format(lib.name)
        l = l + "LIB_SHARED := {}\n".format("1" if lib.shared else "0")

        cf = collect_cflags(lib)
        if cf != "":
            l = l + "CFLAGS += {}\n".format(cf)
        f.write(l)
        f.close()

    if ap.cflags:
        print(collect_cflags(lib))
    
    if ap.name:
        print(lib.name)
    
    if ap.version:
        print(lib.version)
    
    if ap.shared:
        print("1" if lib.shared else "0")


