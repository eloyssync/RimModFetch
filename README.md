# RimModFetch

A lightweight, native C++ Win32 GUI utility to batch-download Steam Workshop mods for RimWorld directly via SteamCMD.

<img width="596" height="486" alt="image" src="https://github.com/user-attachments/assets/1447f690-91a5-4d33-a9fc-9fcb15cac58a" />

### Features
- Native Win32 GUI with zero bloated dependencies.
- Extracts Steam Workshop IDs automatically from raw links, text, or number lists.
- Removes duplicate mod IDs before downloading.
- Automatically reads `About.xml` and displays actual mod names in the status log.
- Copies downloaded mods straight into your RimWorld `Mods` directory.

### Requirements & Setup
1. Download **SteamCMD** from Valve's official site and extract it (default path: `C:\steamcmd\`).
2. Run `steamcmd.exe` once to let it download its core files.
3. Launch **RimModFetch.exe**.
4. Verify paths to `steamcmd.exe` and your RimWorld `Mods` folder.
5. Paste Workshop URLs or IDs into the text box and click **Download & Install**.

*No Steam credentials needed  mod downloads use anonymous authentication.*

### Build from source

**MinGW (GCC):**
```bash
g++ main.cpp -o RimModFetch.exe -mwindows -lcomctl32 -static
