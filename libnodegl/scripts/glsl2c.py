#!/usr/bin/env python

import sys
import os.path as op

fname = sys.argv[1]
c_lines = []
with open(fname) as f:
    for line in f:
        line = line.rstrip()
        line = line.replace('\\', '\\\\')
        line = line.replace('"', '\\"')
        line = f'    "{line}\\n"'
        c_lines.append(line)

name = op.basename(fname).replace('.', '_')
c_code = '\n'.join(c_lines)
print(f'static const char *{name} = \\\n{c_code};')
