#!/bin/sh
# Cross-compile RomanceUnlock.asi (Windows x64 DLL) with clang + lld. No Windows SDK or CRT needed.
set -e
cd "$(dirname "$0")"
DLLTOOL=$(command -v llvm-dlltool-18 || command -v llvm-dlltool)
cat > k32.def <<'DEF'
LIBRARY KERNEL32.dll
EXPORTS
CreateFileW
WriteFile
CloseHandle
GetModuleFileNameW
CreateThread
Sleep
VirtualQuery
VirtualAlloc
GetCurrentProcess
ReadProcessMemory
WriteProcessMemory
GetPrivateProfileIntW
GetFileAttributesW
DisableThreadLibraryCalls
SetThreadPriority
GetCurrentThread
GetTickCount
GetCurrentProcessId
DEF
$DLLTOOL -m i386:x86-64 -d k32.def -l k32.lib
clang-18 -target x86_64-pc-windows-msvc -O2 -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector \
  -fno-asynchronous-unwind-tables -c romance_unlock.c -o romance_unlock.obj
lld-link-18 /dll /nodefaultlib /entry:DllMain /subsystem:windows /opt:ref /out:RomanceUnlock.dll romance_unlock.obj k32.lib
cp RomanceUnlock.dll RomanceUnlock.asi
rm -f romance_unlock.obj k32.lib RomanceUnlock.dll
echo built RomanceUnlock.asi
