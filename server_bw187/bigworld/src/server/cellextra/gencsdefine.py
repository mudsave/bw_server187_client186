#!/usr/bin/env python
# -*- coding: utf_8 -*-
import sys
import optparse
import os

USAGE   = """%python ./gencsdefine.py """.rstrip()

def run(file, path):
    if not os.path.exists(file):
        print "file not exists: %s\n"%(file)

    rfd = open(file, "r")

    wfd = open(path, "w + b")
    wfd.write("#ifndef CSDEFINE_HPP\n")
    wfd.write("#define CSDEFINE_HPP\n")

    lines = rfd.readlines()
    for item in lines:
        if item.find("=") != -1:
            values = item.split("=");
            key = values[0]
            value = str(values[1].split("#")[0]).strip()
            if value.isdigit() or value.startswith("0x"):
                ds = "#define %s %s\n" % (key,value)
                wfd.write(ds)

    rfd.close()

    wfd.write("#endif\n")
    wfd.close()

if __name__ == "__main__":
    opt = optparse.OptionParser( USAGE )
    opt.add_option( "-f", dest = "file", default = "", help = "file" )
    opt.add_option( "-p", dest = "path", default = "", help = "output path" )
    options, args = opt.parse_args()

    if options.path != "" and options.file :
        run(options.file ,options.path)
    else:
        print "python ./gencsdefine.py -f csdefine.py -p csdefine.h"
