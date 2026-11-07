#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
cat > build/kernel32.def <<'DEF'
LIBRARY KERNEL32.dll
EXPORTS
 _OpenFileMappingA@12 = OpenFileMappingA
 _MapViewOfFile@20 = MapViewOfFile
 _UnmapViewOfFile@4 = UnmapViewOfFile
 _CloseHandle@4 = CloseHandle
 _GetTickCount@0 = GetTickCount
DEF
cat > build/user32.def <<'DEF'
LIBRARY USER32.dll
EXPORTS
 _MessageBoxA@16 = MessageBoxA
DEF
lld-link /dll /noentry /machine:x86 /def:build/kernel32.def /out:build/stubkernel32.dll /implib:build/kernel32.lib
lld-link /dll /noentry /machine:x86 /def:build/user32.def /out:build/stubuser32.dll /implib:build/user32.lib
clang -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -c src/ampspread.c -o build/ampspread.obj
lld-link /dll /noentry /machine:x86 /nodefaultlib /def:src/ampspread.def /out:ampspread.dll build/ampspread.obj build/kernel32.lib build/user32.lib
python3 - <<'PY'
p='ampspread.dll'
data=open(p,'rb').read()
for name in ('OpenFileMappingA@12','MapViewOfFile@20','UnmapViewOfFile@4',
             'CloseHandle@4','GetTickCount@0','MessageBoxA@16'):
    old=name.encode()+b'\0'
    assert data.count(old)==1, f'Unexpected DLL import shape: {name}'
    new=name.split('@')[0].encode()+b'\0'
    data=data.replace(old,new.ljust(len(old),b'\0'))
open(p,'wb').write(data)
PY
echo 'Built ampspread.dll (Win32 x86, CRT-free)'
