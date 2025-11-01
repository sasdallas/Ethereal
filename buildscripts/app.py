import sys
import argparse
import subprocess
import os

class App():
    def __init__(self, name, depends, install_dir, symlinks, additional_targets, cflags, ldflags):
        self.name = name
        self.depends = depends
        self.install_dir = install_dir
        self.symlinks = symlinks
        self.additional_targets = additional_targets
        self.cflags = cflags
        self.ldflags = ldflags

def get_line_or_replace(lines, key, rep_value, error=False):    
    for line in lines:
        if line.startswith(key):
            line = line.replace(" = ", "=")
            a = line.split("=")
            return a[1]
        
    if error:
        raise Exception("{} is required in app.conf but was not found".format(key))

    return rep_value

def process_file(fn):
    f = open(fn, "r")
    lines = [line.strip() for line in f.readlines()]
    f.close()

    name = get_line_or_replace(lines, "NAME", "", error=True)
    install_dir = get_line_or_replace(lines, "INSTALL_DIR", "/usr/bin/", error=False)
    if install_dir[-1:] != "/":
        install_dir = install_dir + "/"

    # test sysroot patb exists
    if not os.path.exists(os.path.join(os.environ["SYSROOT"], install_dir)):
        raise Exception("{} does not exist (INSTALL_DIR)".format(install_dir))
    
    depends = get_line_or_replace(lines, "DEPENDS", "", error=False)
    additional_targets = get_line_or_replace(lines, "ADDITIONAL_TARGETS", "", error=False)
    symlink = get_line_or_replace(lines, "SYMLINKS", "", error=False)
    cflags = get_line_or_replace(lines, "CFLAGS", "", error=False)
    ldflags = get_line_or_replace(lines, "LDFLAGS", "", error=False)

    if depends != "":
        depends = depends.replace(" ", "").split(",")
    else:
        depends = []

    if symlink != "":
        symlink = symlink.replace(" ", "").split(",")
    else:
        symlink = []
    
    return App(name, depends, install_dir, symlink, additional_targets, cflags, ldflags)


def get_libs(a: App):
    r = ""
    for i in a.depends:
        r = r + " " + i
    if r == "":
        return ""
    
    o = subprocess.run(["pkg-config", "--libs", r])
    if o.returncode != 0:
        raise Exception("returncode != 0")

    if o.stdout is None:
        return ""
    
    ldflags =  o.stdout.decode("utf-8")
    return ldflags


def get_cflags(a: App):

    r = ""
    for i in a.depends:
        r = r + " " + i
    if r == "":
        return ""
    
    o = subprocess.run(["pkg-config", "--cflags", r])
    if o.returncode != 0:
        raise Exception("returncode != 0")

    if o.stdout is None:
        return ""
    
    cflags = a.cflags + " " + o.stdout.decode("utf-8")
    return cflags

if __name__ == "__main__":
    args = argparse.ArgumentParser()
    args.add_argument("filename")
    args.add_argument("--ldflags", action="store_true")
    args.add_argument("--cflags", action="store_true")
    args.add_argument("--name", action="store_true")
    args.add_argument("--install-dir", action="store_true")
    args.add_argument("--libs", action="store_true")
    args.add_argument("--create-symlinks", action="store_true")
    args.add_argument("--additional-targets", action="store_true")
    
    p  = args.parse_args()
    a = process_file(p.filename)

    if p.libs:
        print(get_libs(a))
    
    if p.ldflags:
        print(a.ldflags)

    if p.cflags:
        print(get_cflags(a))
    
    if p.name:
        print(a.name)
    
    if p.install_dir:
        print(a.install_dir)

    if p.create_symlinks:
        if len(a.symlinks) != 0:

            if "SYSROOT" not in os.environ or os.environ["SYSROOT"] == "":
                raise Exception("$SYSROOT must be set")
            path = os.environ["SYSROOT"]

            

            d = a.install_dir + "/" + a.name
            for sym in a.symlinks:
                try:
                    if not os.path.exists(os.path.dirname(path + sym)):
                        os.mkdir(os.path.dirname(path + sym))
                    os.symlink(d, path + sym)
                except FileExistsError:
                    pass
        
    if p.additional_targets:
        print(a.additional_targets)
