# AmpSpread Afterburner Edition v1.0.4

Adds MSI Afterburner Clear History shortcut integration, while retaining all five sensors and time-weighted averaging from v1.0.3.

## Changes
- Reset the running average and maximum with Afterburner's existing Clear monitoring history shortcut, without restarting the program.
- Detect the configured shortcut in `Profiles\MSIAfterburner.cfg` (with root-config fallback), and display shortcut diagnostics through plugin Setup.
- Give all five sensors distinct monitoring source instance identifiers.
- Preserve v1.0.3's trapezoidal time-weighted average, missing-reading/long-gap exclusion, more consistent shared-memory snapshots, and Data Complete sensor.
- Continue reading only existing PSU.dll monitoring values; no additional PSU polling.

## Validation
The merged v1.0.4 DLL was tested successfully in MSI Afterburner by the project user. Automated synthetic calculation and hotkey checks are included.

## Installation
Completely close Afterburner, then replace `ampspread.dll` in `C:\Program Files (x86)\MSI Afterburner\Plugins\Monitoring\`. Restart and enable the plugin with the built-in `PSU.dll`. Configure Clear monitoring history under Monitoring settings.

## Safety
Read-only monitor; not a replacement for PSU hardware protection. Afterburner's own blue/red history extrema are controlled by the host.
