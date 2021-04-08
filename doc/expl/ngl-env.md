# node.gl standalone build environment

`configure.py` is the root script used to prepare a standalone node.gl virtual
environment for casual users and developers. If you are a packager, you
probably do **not** want to use this script and instead package every component
independently using their respective build systems (`meson` for `libnodegl` and
`ngl-tools`, `pip` for `pynodegl` and `pynodegl-utils`).

The goal of this script is to have an isolated environment without being
intrusive on the host system, being as simple and straightforward as possible
for the end-user. The script is written in Python3 so you will need to have it
installed on your system.

`./configure.py -h` displays all the controls available for that environment.
This command by default creates a Python Virtual environment in the `venv`
directory (unless customized to something else with the `-p` option). In this
virtual environment, `pip` is installed, along with `meson` (needed for
building some of our components) and various other module requirements. A few
external dependencies such as `sxplayer` are then pulled, and a `Makefile` is
generated.

The generated `Makefile` (compatible `NMake` on Windows and `GNU/Make` on other
systems) provides a build chain for compiling and installing the node.gl
components within this environment.

Running `make` (or `nmake`) is re-entrant, so developers can do their
modifications to the code and iterate easily. It is possible to iterate faster
by selecting the specific build chain rule, for example: `make nodegl-install`.

After the build, it is possible to enter the environment with the provided
activation command to access the tools (`ngl-control`, `ngl-desktop`, etc.), as
well as importing `pynodegl` and `pynodegl-utils` within Python.

The temporary build files are located in `builddir`. This means that if the
virtual env is activated, you can also typically manually run `meson` commands
from here.
