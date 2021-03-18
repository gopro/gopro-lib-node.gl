# ngl-env

`ngl-env` is the root script used to create and manage a standalone node.gl
virtual environment for casual users and developers. If you are a packager, you
probably do **not** want to use this script and instead package every component
independently using their respective build systems (`meson` for `libnodegl` and
`ngl-tools`, `pip` for `pynodegl` and `pynodegl-utils`).

The goal of this script is to have an isolated environment without being
intrusive on the host system, being as simple and straightforward as possible
for the end-user. The script is written in Python3 so you will need to have it
installed on your system.

The main command is `build` (see `./ngl-env build -h`). This command creates a
Python Virtual environment in the `venv` directory (unless customized to
something else with the `-p` option). In this virtual environment, `pip` is
installed, along with `meson` (needed for building some of our components) and
various other module requirements. A few external dependencies such as
`sxplayer` are then pull, compiled and installed within this environment,
followed by all the node.gl components.

The `build` command is re-entrant, so a developer can modify the code and
iterate using this command. It is possible to iterate faster by selecting the
precise component you want to rebuild using the `-t` (target) option, for
example: `./ngl-env build -t libnodegl`.

After the build, it is possible to enter the environment with the provided
activation command to access the tools (`ngl-control`, `ngl-desktop`, etc.), as
well as importing `pynodegl` and `pynodegl-utils` within Python.

The temporary build files are located in `builddir`. This means that if the
virtual env is activated, you can also typically manually run `meson` commands
from here.

`ngl-env` also allows to run the test suite (`tests`), clean the sources
(`clean`), and access various developers `dev` commands (amongst other things).

For more information, see `./ngl-env --help`.
