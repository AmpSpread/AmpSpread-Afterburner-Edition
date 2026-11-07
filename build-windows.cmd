@echo off
setlocal
rem Run from a Visual Studio 2022 x86 Native Tools Command Prompt.
cd /d "%~dp0"
where cl >nul 2>nul || (echo cl.exe not found. Open x86 Native Tools Command Prompt.& exit /b 1)
cl /nologo /O2 /W4 /TC /LD /MD src\ampspread.c /link /nologo /MACHINE:X86 /DEF:src\ampspread.def /OUT:ampspread.dll kernel32.lib user32.lib
if errorlevel 1 exit /b 1
echo Built ampspread.dll
