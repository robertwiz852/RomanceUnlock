# RomanceUnlock

An ASI plugin for Crimson Desert (build 2.04 with the Charting the Unknown DLC) that makes the Graymane camp NPCs romanceable.

It does not modify any game file or save. While the game runs it scans the game's own memory for NPC character records, checks that each one has the expected layout, and changes two bytes on the matching NPCs. Closing the game undoes everything.

## What it does exactly

- Finds NPC objects in memory (a pointer to a name node, a key, and a fixed byte pattern).
- Checks the name starts with `N`, then `H`/`O`/`D`, then `W` or `M`, then `_` (human or camp NPC records).
- Sets the "marriageable" flag (byte at offset 36 of the object) to 1.
- For camp NPCs that have a second flag set to 4 (offset 62), sets it to 3, which is the value romanceable NPCs have.
- Rescans every few seconds because the game loads NPCs in and out.
- Uses only kernel32 calls: file writes for its log, memory queries (`VirtualQuery`), `ReadProcessMemory` / `WriteProcessMemory` on its own process, and ini reading. No network, no process injection into other programs, no files other than its own `.log` and `.ini`.
- Only runs inside `CrimsonDesert.exe`. In any other process the DLL exits immediately.

## Settings (`RomanceUnlock.ini`)

| Mode | Meaning |
|------|---------|
| 5 | Camp plus all other adult humans (default) |
| 4 | Camp crew only |
| 1 | Joan only |
| 0 | Off |

## Files

- `romance_unlock.c` : the whole plugin
- `core.h`, `stubs.h` : helper types and test stubs
- `test_rom.c` : native tests that run the scanner against a fake memory layout
- `build.sh` : build script

## Building

The plugin is a freestanding 64-bit Windows DLL with no C runtime. It is cross-compiled with clang and lld, so no Windows SDK or Visual Studio is needed. Tested on Ubuntu 24.04 (also works in WSL on Windows).

1. Install the tools:
   ```
   sudo apt update
   sudo apt install clang-18 lld-18 llvm-18
   ```
2. Build:
   ```
   sh build.sh
   ```
   This produces `RomanceUnlock.asi` in the same folder.
3. Optional, run the native tests (needs a normal C compiler):
   ```
   cc -O1 -w -o test_rom test_rom.c && ./test_rom
   ```

## Installing

1. Put an ASI loader such as [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) in the game's `bin64` folder (x64 build, renamed to `winmm.dll` or `version.dll`).
2. Copy `RomanceUnlock.asi` and `RomanceUnlock.ini` into `bin64`.
3. Start the game and wait up to a minute after loading.

## Notes

- Back up your saves before marrying anyone.
- A game update may change the data layout and break the plugin. It checks the layout before writing and does nothing if it does not match.
- Some antivirus engines flag small unsigned DLLs that read and write process memory. This is a false positive; the full source is here.

## License

MIT, see `LICENSE`.
