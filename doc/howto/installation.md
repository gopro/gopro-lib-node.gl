# Installation

This page includes the system requirements as well as the steps needed for
building and running the complete `node.gl` stack.

## Quick user installation on Linux and MacOS

- Install the needed dependencies:
  - **GCC** or **Clang**
  - **GNU/Make**
  - **FFmpeg** (and its libraries for compilation)
  - **Python 3.x** (you will need the package with the build headers as well,
    typically named with a `-devel` suffix on Debian based systems)
  - **pip** (Python 3 version)
  - **Graphviz**
  - **SDL2**
- Build with `make -jN` where `N` is the number of parallel processes
- Enter the virtual environment with `. venv/bin/activate`

## Quick user installation on Windows (MinGW64 toolchain)

- Install [MSYS2](https://www.msys2.org/) (which also brings **MinGW64**)
- Install dependencies via pacman using **MinGW64** shell (*Not* MSYS2,
"MINGW64" should be visible in the prompt):
    ```shell
    pacman -Syuu  # and restart the shell
    pacman -S git make
    pacman -S mingw-w64-x86_64-{toolchain,ffmpeg,python}
    pacman -S mingw-w64-x86_64-python-watchdog
    pacman -S mingw-w64-x86_64-python3-{pillow,pip}
    pacman -S mingw-w64-x86_64-pyside2-qt5
    pacman -S mingw-w64-x86_64-meson
    pacman -S mingw-w64-x86_64-graphviz
    ```
- From MinGW64, build with `make -jN TARGET_OS=MinGW-w64` where `N` is the number of parallel processes
- Enter the virtual environment with `. venv/bin/activate`

## Quick user installation on Windows (MSVC toolchain)

- Install [Python](https://www.python.org/downloads/windows/) 3.x using the Windows Installer
- Install [Graphviz](https://graphviz.org/download/) using the Windows Installer and make sure to select the option to add Graphviz
to the system PATH during its installation
- Install [Microsoft Visual Studio](https://visualstudio.microsoft.com/downloads/) Community 2019 or greater and make sure the
following components are included:
    - Desktop development with C++
    - MSVC - VS 2019 C++ x64/x86 build tools
    - Windows 10 SDK
- Install [Vcpkg](https://github.com/microsoft/vcpkg) from Windows PowerShell as of commit 09a647a (see issue [16658](https://github.com/microsoft/vcpkg/issues/16658)):
    ```shell
    git.exe checkout 09a647a5261e981cbbc8219292c838490ed0b06f -b shaderc/workaround-16658
    .\bootstrap-vcpkg.bat
    .\vcpkg.exe install pthreads:x64-windows opengl-registry:x64-windows ffmpeg[ffmpeg,ffprobe]:x64-windows sdl2:x64-windows
    .\vcpkg.exe integrate install
    ```
- Install [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) (optional)
- Enable [WSL (Windows Subsystem for Linux)](https://docs.microsoft.com/en-us/windows/wsl/install-win10)
- Install Ubuntu into WSL from Microsoft Store.
- In WSL, install other needed dependencies as
following:
    ```shell
    sudo apt -y update
    sudo apt -y install build-essential unzip
    ```
- Add `C:\vcpkg\installed\x64-windows\tools\ffmpeg` path to your windows system `%PATH%` environment variable (`ffmpeg`
and `ffprobe` binaries must be available in order to run the tests)
- From WSL, build with `make -jN TARGET_OS=Windows` where `N` is the number of parallel processes
- From WSL, access powershell with `powershell.exe`
- Now from powershell, you can enter the virtual environment with:
    ```shell
    set-executionpolicy RemoteSigned # to allows special scripts to run
    .\venv\Scripts\Activate.ps1
    ```

### Known limitations

Even if both WSL versions have been tested, we encourage to use version 1
which seems for now faster and more stable in our use case. Indeed we are
accessing executable files mounted from Windows, which is a drawback for
[WSL 2 linux kernel needing files entirely stored in Linux file system to
be efficient][wsl1-vs-wsl2].

[wsl1-vs-wsl2]: https://docs.microsoft.com/en-us/windows/wsl/compare-versions#exceptions-for-using-wsl-1-rather-than-wsl-2

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
