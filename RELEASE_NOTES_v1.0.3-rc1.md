# AmpSpread Afterburner Edition v1.0.3-rc1 — Test build

This is a **prerelease** to validate updated monitoring behavior in MSI Afterburner 4.6.7 Beta 3. Version 1.0.2 remains the stable release.

## What's improved
- Running average is now **elapsed-time-weighted** (trapezoidal integration of published spread values), reducing dependence on 100 ms versus 250 ms Afterburner callback rates.
- Consecutive invalid snapshots and gaps longer than two seconds are excluded from the time-weighted average.
- Added optional monitoring sensor **12V-2x6 data complete** (1 = a complete six-pin set is available, 0 = unavailable/incomplete). Existing first four sensor indices/names are unchanged.
- Shared-memory snapshots double-check all six pin values to reduce inconsistent reads while Afterburner is updating them.
- Reduces same-cycle callback caching from 50 ms to 25 ms. This does **not** guarantee elimination of Afterburner/PSU monitoring latency.

## Still controlled externally
- Previously saved graph ranges are owned by MSI Afterburner and cannot be safely overridden by a read-only monitoring DLL. The defaults remain live 0–1 A, average 0–0.5 A, maximum 0–1 A.
- No true PSU sampling sequence number is present in the Afterburner shared-memory interface. A time-weighted mean of *reported readings* is not guaranteed to equal a mean of all physical PSU measurements.
- Lag introduced by MSI's polling / shared memory publication remains possible; this DLL never performs USB PSU reads.
- Monitoring cannot prove a connector safe or substitute for physical temperature, seating, or PSU hardware safeguards.
- This build remains **unsigned**; it cannot be authenticode-signed without a valid signing certificate.

## Test before replacing your stable copy
Close Afterburner, back up v1.0.2, replace ampspread.dll in Plugins/Monitoring, then restart. Compare idle and loaded readings. Reset between cable tests via plugin Setup.

Automated tests cover: ABI descriptions, graph ranges, six-pin current spread, time-weighted average, maximum, dual ports, reset, missing data, interruption/gaps, and shared-memory bounds.
