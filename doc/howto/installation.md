# Installation

## Dependencies

Make sure you have installed on your system the essential list of dependencies
for building and running the complete **node.gl** stack:

- **GCC** or **Clang**
- **GNU/Make**
- **FFmpeg** (and its libraries for compilation)
- **Python 3.x** (you will need the package with the build headers as well,
  typically named with a `-devel` suffix on Debian based systems)
- **pip** (Python 3 version)
- **Graphviz**
- **SDL2**

## Quick user installation on Linux and MacOS

The following steps describe how to install `node.gl` and its dependencies in
your home user directory, without polluting your system (aside from the system
dependencies which you should install and remove in sane ways).

For a fast ready-to-go user experience, you can use the default rule of the
root `Makefile`.  Calling `make` in the root directory of node.gl will create a
complete environment (based on Python `venv` module):

```shell
% make
[...]
    Install completed.

    You can now enter the venv with:
        . /home/user/node.gl/nodegl-env/bin/activate
```

Jobbed `make` calls are supported, so you can use `make -jN` where `N` is the
number of parallel processes.

**Note**: to leave the environment, you can use `deactivate`.

## Quick user installation on Windows

On Windows, the bootstrap is slightly more complex:

- install [MSYS2](https://www.msys2.org/)
- run MinGW64 shell (*NOT* MSYS2, "MINGW64" should be visible in the prompt)
- run the following in the shell:
```shell
pacman -Syuu  # and restart the shell
pacman -S git make
pacman -S mingw-w64-x86_64-{toolchain,ffmpeg,python}
pacman -S mingw-w64-x86_64-python3-pip
pacman -S mingw-w64-x86_64-meson
make TARGET_OS=MinGW-w64
```

Then you should be able to enter the environment and run the tools.

## Installation of `libnodegl` (the core library)

`libnodegl` uses [Meson][meson] for its build system. Its compilation and
installation usually looks like the following:

```sh
meson setup builddir
meson compile -C builddir
meson install -C builddir
```

`meson configure` can be used to list the available options. See the [Meson
documentation][meson-doc] for more information.

[meson]: https://mesonbuild.com/
[meson-doc]: https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project

## Installation of `ngl-tools`

The `node.gl` tools located in the `ngl-tools/` directory are to be built and
installed exactly the same way as `libnodegl`.

## Installation of `pynodegl` (the Python binding)

```shell
pip install -r ./pynodegl/requirements.txt
pip install ./pynodegl
```

## Installation of `pynodegl-utils` (the Python utilities and examples)

```shell
pip install -r ./pynodegl-utils/requirements.txt
pip install ./pynodegl-utils
```
