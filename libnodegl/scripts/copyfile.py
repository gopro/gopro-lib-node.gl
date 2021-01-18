#!/usr/bin/env python3
"""
Usage: copyfile.py SOURCE... DIRECTORY
Copy multiple SOURCE(s) to DIRECTORY.

Arguments:
    SOURCE(s) -- list of source files
    DIRECTORY -- Output folder where to copy source files
"""

import os
import sys
import shutil

if len(sys.argv) < 2:
    print(__doc__)
    exit(1)

dst = sys.argv[-1]
if len(sys.argv) > 2 and not os.path.isdir(dst):
    print(__doc__)
    exit(1)

for src in sys.argv[1:-1]:
    shutil.copy2(src, dst)
