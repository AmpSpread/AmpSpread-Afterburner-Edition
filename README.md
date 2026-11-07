# AmpSpread Afterburner Edition

### [⬇ Download AmpSpread Afterburner Edition v1.0.2 (ampspread.dll)](https://github.com/AmpSpread/AmpSpread-Afterburner-Edition/releases/download/v1.0.2/ampspread.dll)

[View the latest release and installation files](https://github.com/AmpSpread/AmpSpread-Afterburner-Edition/releases/latest)

**Standalone, read-only 12V-2×6 current-balance monitoring for MSI Afterburner.**

## Download

**[Download AmpSpread.dll — v1.0.2 (Windows, 32-bit)](https://github.com/AmpSpread/AmpSpread-Afterburner-Edition/releases/download/v1.0.2/ampspread.dll)**

[View release notes and downloads](https://github.com/AmpSpread/AmpSpread-Afterburner-Edition/releases/tag/v1.0.2)

AmpSpread Afterburner Edition adds live, average and maximum **per-pin current spread** graphs to MSI Afterburner's hardware monitor. It uses the six current readings already supplied by MSI's `PSU.dll` plugin. You do **not** need to run the separate AmpSpread desktop application.

![AmpSpread Afterburner Edition showing live, average, and maximum 12V-2x6 current spread](assets/image.png)

*Screenshot: the plugin running in MSI Afterburner 4.6.7 Beta 3, reading an MSI MPG Ai1600TS PSU.*

## Monitoring graphs

| Graph | Meaning | Default range |
| --- | --- | --- |
| **12V-2x6 current spread** | Current sample: highest of six pins minus lowest | 0–1.000 A |
| **12V-2x6 average spread** | Running arithmetic mean of recorded spread samples | 0–0.500 A |
| **12V-2x6 maximum spread** | Largest spread recorded during this monitoring session | 0–1.000 A |
| **12V-2x6 active port** | Automatically selected PSU 12VHPWR port (1 or 2) | 0–2 |

All spread values display to three decimal places. Independent running statistics are retained for each PSU port; if both are populated, the complete six-pin group with the higher total current is selected.

## Requirements

- MSI Afterburner for Windows with support for 32-bit monitoring plugins (tested by a user with **4.6.7 Beta 3**).
- MSI `PSU.dll` monitoring plugin enabled and publishing six per-pin currents for one 12VHPWR PSU port (for example, `PSU 12VHPWR1_1 current` through `PSU 12VHPWR1_6 current`, in amperes).
- A compatible PSU reporting those currents; originally developed for the **MSI MPG Ai1600TS**.

## Installation

1. Completely exit MSI Afterburner, including its system-tray process.
2. Download and copy [`ampspread.dll`](https://github.com/AmpSpread/AmpSpread-Afterburner-Edition/releases/download/v1.0.2/ampspread.dll) into:
   `C:\Program Files (x86)\MSI Afterburner\Plugins\Monitoring\`
3. (Optional) Copy [`Help/Plugins/Monitoring/ampspread`](Help/Plugins/Monitoring/ampspread) into the corresponding Afterburner `Help\Plugins\Monitoring\` directory to add the plugin description.
4. Restart MSI Afterburner. Open **Settings → Monitoring → [...]** and check **ampspread.dll**.
5. In the monitoring sensor list, enable the AmpSpread graphs you want to display.

**Upgrading from PSUSpread:** remove the old `PSUSpread.dll` first to prevent duplicate monitoring sensors.

## Reset for a cable comparison

Open **Settings → Monitoring → [...] → ampspread.dll → Setup → Yes**. This resets the session average and maximum for both PSU ports, without changing PSU settings or Afterburner's hardware polling. Restarting Afterburner also resets the session.

For meaningful comparisons, keep GPU load, Afterburner polling interval, test duration and ambient conditions as consistent as practical.

## How it works

```text
MSI PSU telemetry → MSI's PSU.dll → Afterburner MAHMSharedMemory
                                         ↓ (read-only)
                                   ampspread.dll
                                         ↓
                      Live / average / maximum spread graphs
```

The DLL opens Afterburner's shared-memory mapping as **read-only**. It never communicates directly with the PSU, installs drivers, starts services, controls the GPU power limit or modifies Safeguard+ settings.

## Test build: v1.0.3-rc1

[Download v1.0.3-rc1 prerelease DLL](https://github.com/AmpSpread/AmpSpread-Afterburner-Edition/releases/download/v1.0.3-rc1/ampspread.dll) — experimental; v1.0.2 above remains the stable download.

Changes: **time-weighted average** (less polling-rate bias), exclusion of missing-data gaps, six-pin snapshot recheck, and optional **data complete** sensor (1/0). Current, average, maximum, and active-port names remain unchanged.

## Remaining limitations

- Changing the DLL's default graph ranges won't necessarily overwrite previously saved graph limits in Afterburner.
- The prerelease's elapsed-time-weighted mean reduces callback-rate bias but does **not** distinguish an unchanged physical reading from a duplicate published reading. PSU sample timestamps/sequence IDs are not exposed. Keep polling and test conditions consistent for A/B comparisons.
- Afterburner controls the order/timing of its PSU readings and shared-memory publication. The DLL cannot guarantee elimination of up to one polling interval of graph lag without independent hardware access.
- Missing or incomplete per-pin data is intentionally reported as unavailable (not 0 A); the prerelease adds a separate **data complete** status sensor. Completeness is not a guarantee of hardware freshness.
- A small current spread does **not** establish that a power connector is safe: contact resistance, seating, temperature and other faults also matter. This plugin is **not** a protection or shutdown device.
- The DLL is **unsigned** until the project obtains a trusted code-signing certificate. A published SHA-256 checksum helps verify file integrity but is not a substitute for Authenticode signing. Don't disable Windows security features solely to run it.

## Build and test

The C source is in [`src/ampspread.c`](src/ampspread.c) and its exports in [`src/ampspread.def`](src/ampspread.def).

On Windows, run `build-windows.cmd` from a **Visual Studio 2022 x86 Native Tools Command Prompt** with C++ build tools installed. For cross-compiling on Linux, use `build-linux.sh` (Clang, lld-link and Python 3).

Synthetic spread tests are in [`tests/test_spread.c`](tests/test_spread.c). The plugin has been demonstrated running in MSI Afterburner 4.6.7 Beta 3 (see screenshot). This is not an official MSI project.

## License

MIT — see [LICENSE](LICENSE).

**AmpSpread Afterburner Edition** is separate from the full AmpSpread desktop application.