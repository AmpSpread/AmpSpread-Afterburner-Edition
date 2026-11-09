# Changelog

## v1.0.4 — November 9, 2026

- Integrates with MSI Afterburner's configured **Clear monitoring history** hotkey to reset AmpSpread running average and maximum, without restarting the application.
- Reads `ClearHistoryHotkey` from `Profiles\MSIAfterburner.cfg`, falling back to the installation-root config.
- Displays hotkey detection and reset-count diagnostics through the plugin's Setup dialog.
- Assigns independent sensor instances for improved host graph Min/Max history behavior.
- Preserves all five monitoring sensors, including the Data Complete indicator.
- Retains the elapsed-time-weighted average, missing-data/gap exclusion, and shared-memory snapshot checks introduced in v1.0.3.
- Continues to operate read-only without a second PSU polling service or the full AmpSpread application.

The combined v1.0.4 test DLL was confirmed working in MSI Afterburner by the project user. Host-generated blue/red historical Min/Max annotations may vary with Afterburner's behavior.

## v1.0.3

- Time-weighted average, fifth Data Complete sensor, gap handling, and improved shared-memory snapshots.

## v1.0.2

- Initial standalone monitoring release, including live, average and maximum spread plus active port.
