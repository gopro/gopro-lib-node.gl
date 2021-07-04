#!/usr/bin/env python

import re
import os.path as op


_include_re = re.compile(r'\s*#include\s+(?P<filename>.*)$')


def _get_c_lines(fname):
    c_lines = []
    with open(fname) as f:
        for line in f:
            line = line.rstrip()

            include_match = _include_re.match(line)
            if include_match:
                inc_fname = include_match.group('filename')
                inc_path = op.join(op.dirname(fname), inc_fname)
                c_lines += _get_c_lines(inc_path)
                continue

            line = line.replace('\\', '\\\\')
            line = line.replace('"', '\\"')
            line = f'    "{line}\\n"'
            c_lines.append(line)
    return c_lines


def _run(fname):
    c_lines = _get_c_lines(fname)
    name = op.basename(fname).replace('.', '_')
    c_code = '\n'.join(c_lines)
    print(f'static const char *{name} = \\\n{c_code};')


if __name__ == '__main__':
    import sys
    _run(sys.argv[1])
