# Quick user build & installation

This page includes the system requirements as well as the steps needed for
building and running the complete `node.gl` stack.

## Quick user installation on Linux and MacOS

- Install the needed dependencies:
  - **GCC** or **Clang**
  - **GNU/Make**
  - **FFmpeg** (and its libraries for compilation)
  - **Python ≥ 3.8** (you will need the package with the build headers as well,
    typically named with a `-devel` suffix on Debian based systems)
  - **Graphviz**
  - **SDL2**
- Build with `./configure.py && make`
- Enter the virtual environment with `. venv/bin/activate`

## Quick user installation on Windows (MinGW64 toolchain)

- Install [MSYS2](https://www.msys2.org/) (which also brings **MinGW64**)
- Install dependencies via pacman using **MinGW64** shell (*Not* MSYS2,
"MINGW64" should be visible in the prompt):
    ```shell
    pacman -Syuu  # and restart the shell
    pacman -S git make mingw-w64-x86_64-{toolchain,ffmpeg,python,python-watchdog,python3-pillow,pyside2-qt5,meson,graphviz}
    ```
- From MinGW64, build with `./configure.py && make`
- Enter the virtual environment with `. venv/bin/activate`

## Quick user installation on Windows (MSVC toolchain)

- Install [Python](https://www.python.org/downloads/windows/) ≥ 3.8 using the
  Windows Installer and make sure to select the option to add Python to the
  system `PATH` during its installation
- Install [Graphviz](https://graphviz.org/download/) using the Windows
  Installer and make sure to select the option to add Graphviz to the system
  `PATH` during its installation
- Install [Microsoft Visual
  Studio](https://visualstudio.microsoft.com/downloads/) Community 2019 or
  greater and make sure the *Desktop development with C++* is included (it
  should enable MSVC build tools and Windows 10 SDK).
- Download and extract [Vcpkg](https://github.com/microsoft/vcpkg) in
  `C:\vcpkg`, then from Windows PowerShell:
    ```shell
    .\bootstrap-vcpkg.bat
    .\vcpkg.exe install --triplet x64-windows opengl-registry ffmpeg[ffmpeg,ffprobe] sdl2
    ```
- Add `C:\vcpkg\installed\x64-windows\tools\ffmpeg` path to your windows system
  `%PATH%` environment variable (`ffmpeg` and `ffprobe` binaries must be
  available in order to run the tests)
- To be allowed to run the build scripts, you will need to run this once in an
  administrator Powershell: `Set-ExecutionPolicy RemoteSigned`
- In a Powershell (as user), in the `node.gl` sources, run
  `.\scripts\msvc-env.ps1` to import all the necessary variables for the MSVC
  environment. Alternatively, you can open *VS Native Tools Prompt* and spawn a
  `powershell` from here.
- Finally, run `python.exe .\configure.py` followed by `nmake`


# Manual components installation

This section is targeted at packagers willing to build packages for every
individual components.

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
