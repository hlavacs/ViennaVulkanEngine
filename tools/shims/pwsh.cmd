@echo off
setlocal
REM Local shim for environments where PowerShell 7 (pwsh.exe) is not installed.
REM vcpkg/MSBuild probes pwsh.exe first; forward to Windows PowerShell instead.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" %*
