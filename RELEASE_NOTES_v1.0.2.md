# AmpSpread Afterburner Edition v1.0.2 — Initial Public Release

A lightweight, **standalone MSI Afterburner monitoring DLL** for observing MSI PSU 12V-2×6 per-pin current balance. No separate AmpSpread app, additional USB polling, driver, or background service is required.

### Monitoring features

- **Live current spread:** highest of the six pin currents minus lowest, in amperes.
- **Average spread:** running average across sampled monitoring readings.
- **Maximum spread:** maximum observed spread since reset.
- **Active port:** automatically uses 12VHPWR port 1 or 2, retaining separate session statistics.
- 3-decimal amp values and improved default graph limits (0–1 A for live/max, 0–0.5 A for average).
- Reset session statistics with **Settings → Monitoring → [...] → ampspread.dll → Setup**.

### Screenshot

![AmpSpread Afterburner Edition monitoring graphs](https://raw.githubusercontent.com/AmpSpread/AmpSpread-Afterburner-Edition/main/assets/afterburner-current-spread.png)

*Captured in MSI Afterburner 4.6.7 Beta 3 using MSI Ai1600TS PSU telemetry.*

### Installation

1. Exit MSI Afterburner completely.
2. Place `ampspread.dll` in `C:\Program Files (x86)\MSI Afterburner\Plugins\Monitoring\`.
3. Start Afterburner and enable `ampspread.dll` under **Settings → Monitoring → [...]**.
4. Optionally copy the included help-description file to Afterburner's corresponding `Help\Plugins\Monitoring\` directory.

Requires MSI's `PSU.dll` monitoring plugin and a compatible PSU exposing all six pin currents. If upgrading from PSUSpread, remove `PSUSpread.dll` to avoid duplicate graphs.

### Limitations and safety

This is a **read-only monitoring tool**, not a safety shutdown or connector-protection system. Readings can be updated no faster than Afterburner/PSU.dll provide them. Low current imbalance alone does not prove connector safety. The included DLL is unsigned; do not bypass your operating system's security protections to run it.

**Files:** `ampspread.dll` (precompiled Win32 DLL), complete source and build scripts, optional plugin description, and screenshot.