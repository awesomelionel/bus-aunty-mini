# Arrival fixtures

Captured from `https://api.busaunty.com/api/v2/BusArrival` on 2026-09-28.
Each live file has all three NextBus slots and the `IsLoop` value the API
returned.

| File | Stop | UpdatedAt | Notes |
| --- | --- | --- | --- |
| `stop_52109.json` | 52109 | 2026-09-28T18:04:08+08:00 | 125 visit 2 is a loop, Type SD |
| `stop_52049.json` | 52049 | 2026-09-28T18:04:08+08:00 | 21 and 129 are two directions; 456 is a loop |
| `stop_66271.json` | 66271 | 2026-09-28T18:04:11+08:00 | service 136, two directions, not a loop |
| `figure_eight.json` | 75009 Tampines Int | 2026-09-28T18:05:06+08:00 | 291 and 293, each one loop entry with visit 1 and visit 2 |
| `stop_52109_125dd.json` | 52109 | 2026-09-28T17:18:59+08:00 | Earlier capture the same day. 125 visit 2 was Type DD. The 18:04 capture is SD, so the double-decker renders use this file. |

These three are synthetic. A live capture does not show the case:

- `terminating.json` — every timed arrival on a row is `Terminating: true`
- `truncate_visit2.json` — a visit-2 label long enough to cut before ` 2nd`
- `truncate_long.json` — a single label that hard-cuts

The 48-service and 96-row caps are built in `test_service_and_row_caps_hold`,
not stored as a file.
