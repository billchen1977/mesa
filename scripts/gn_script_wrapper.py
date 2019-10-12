#!/usr/bin/env python2.7
#all this script does is execute the second argument and pipe its output to the file specified by the first argument
#this is to let GN use the mesa python scripts that generate headers and barf them to stdout
import subprocess
import sys

if __name__ == '__main__':
    f = open(sys.argv[2], 'w')
    exec_string = "PYTHONPATH=" + sys.argv[1] + " python " + " ".join(sys.argv[3:])
    child = subprocess.Popen(exec_string, shell=True, universal_newlines=True, stdout=subprocess.PIPE)
    f.write(child.stdout.read())
    exit(child.returncode)
