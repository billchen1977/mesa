#!/usr/bin/env python
# This script executes the second argument with PYTHONPATH set to the first argument.
import subprocess
import sys

if __name__ == '__main__':
    exec_string = "PYTHONPATH=" + sys.argv[1] + " python " + " ".join(sys.argv[2:])
    child = subprocess.Popen(exec_string, shell=True, stdout=subprocess.PIPE)
    exit(child.returncode)