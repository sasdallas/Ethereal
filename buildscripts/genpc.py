#!/usr/bin/python3
import sys
import os
import lib
import subprocess

# genpc.py - Generate PC files from library configuration

if len(sys.argv) < 2:
    print("Usage: genpc.py [file]")
    sys.exit(1)

if "SYSROOT" not in os.environ or os.environ["SYSROOT"] == "":
    print("$SYSROOT must be set")
    sys.exit(1)

l = lib.create_library_from_info_file(sys.argv[1])

n = l.name[3:] # strip lib prefix
path = os.environ["SYSROOT"] + os.environ["PREFIX"] + "/lib/pkgconfig/"

try:
    os.mkdir(path)
except Exception:
    pass

path = path + n + ".pc"

f = open(path, "w+")


data = ""
data = data + "prefix={}\n".format(os.environ["PREFIX"])
data = data + "exec_prefix=${prefix}\n"
data = data + "libdir={}/lib\n".format(os.environ["PREFIX"])
data = data + "includedir={}/include\n\n".format(os.environ["PREFIX"])
data = data + "Name: {}\n".format(n)
if l.desc != "":
    data = data + "Description: {}\n".format(l.desc.replace("\"", ""))
data = data + "Requires:\nRequires.private:\n"
data = data + "Version: {}\n".format(l.version)
data = data + "Libs: -L${libdir} -l" + n + " "

for i in l.deps:
    data = data +  subprocess.check_output(["pkg-config", "--libs", i]).decode('utf-8').strip() + " "
data = data + "\n"


data = data + "Libs.private: "

for i in l.deps_private:
    data = data + "-l" + i + " "
data = data + "\n"

data = data + "Cflags: -I${includedir} "
data = data + "{}\n".format(l.cflags)

f.write(data)
f.close()