#!/usr/bin/bash
echo "starting msvc environment"
if [[ ! -v VCVARS64 ]]; then
  export VCVARS64='"'$(powershell.exe build_scripts/windows/find_vcvars64.ps1)'"'
fi
# Set Visual Studio environment variables
shift
cmd="WSLENV=VCVARS64/w cmd.exe /C %VCVARS64% \&\& wsl.exe $@"
echo "running command: $cmd"
eval $cmd
