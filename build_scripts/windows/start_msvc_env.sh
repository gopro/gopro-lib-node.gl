#!/usr/bin/bash
set -x
set -e
echo "starting msvc environment"
if [[ ! -v VCVARS64 ]]; then
    basedir=$(dirname "$0")
    export VCVARS64='"'$(powershell.exe $basedir/find_vcvars64.ps1)'"'
    echo $VCVARS64
fi
# Set Visual Studio environment variables and launch wsl
WSLENV=VCVARS64/w cmd.exe /C %VCVARS64% \&\& wsl.exe $@
