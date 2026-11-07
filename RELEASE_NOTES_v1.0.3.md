# AmpSpread Afterburner Edition v1.0.3

**Stable release.** Promotes the tested v1.0.3-rc1 monitoring improvements without changes to the core spread calculation.

## Improvements over v1.0.2

- **Time-weighted average:** reports an elapsed-time-weighted mean of published spread readings, instead of weighting each callback equally. This reduces differences caused by changing Afterburner's polling interval.
- **Gap handling:** invalid readings and monitoring gaps longer than two seconds are excluded from the running time-weighted average.
- **More consistent snapshots:** rechecks the six reported pin currents while reading shared memory to reduce inconsistent mid-update values.
- **Data-complete sensor:** adds `12V-2x6 data complete` (1 = complete six-pin readings available; 0 = unavailable/incomplete).
- **Reduced callback caching:** uses 25 ms instead of 50 ms, reducing the plugin's own potential refresh delay.

The first four sensor names remain unchanged: live spread, average spread, maximum spread, and active port. AmpSpread remains a **standalone, read-only plugin**, using MSI's existing PSU monitoring values with **no additional direct PSU polling**.

## Install / upgrade

1. Quit MSI Afterburner completely, including its tray icon.
2. Back up your existing `ampspread.dll`.
3. Copy the DLL attached to this release into `C:\Program Files (x86)\MSI Afterburner\Plugins\Monitoring\`, replacing the older file.
4. Restart Afterburner. Enable `ampspread.dll` under **Settings → Monitoring → Plugins** if needed.
5. Select **Setup** for the plugin to reset the recorded session when comparing cables.

The DLL is a 32-bit Windows monitoring plugin and remains unsigned.

## Checks

- Automated tests cover spread, elapsed-time-weighted average, maximum, port selection, resets, data availability, gap handling, limits, and shared-memory bounds.
- GitHub Actions compiles the DLL and publishes a `.sha256` checksum alongside the download.
- The preceding v1.0.3-rc1 build was tested successfully in MSI Afterburner by the project user.

## Notes and limitations

The monitoring values depend on MSI Afterburner's PSU polling and shared-memory publication. Existing manually saved graph limits may override the defaults. A balanced current spread does **not** guarantee connector safety; this plugin is not a thermal protection device. Do not disable Windows security features solely to load an unsigned DLL.
