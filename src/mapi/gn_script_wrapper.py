#!/usr/bin/env python
#all this script does is execute the second argument and pipe its output to the file specified by the first argument
#this is to let GN use the mesa python scripts that generate headers and barf them to stdout
import subprocess
import sys

if __name__ == '__main__':
    f = open(sys.argv[1], 'w')
    exec_string = "python " + " ".join(sys.argv[2:])
    child = subprocess.Popen(exec_string, shell=True, stdout=subprocess.PIPE)
    print f.write(child.stdout.read())
    exit(child.returncode)