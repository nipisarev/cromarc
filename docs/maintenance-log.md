# Maintenance log

Bumps of the Chromium pin (design, "Цикл обновлений Chromium").

| Date | From | To | Sync | Build | Total | Notes |
|---|---|---|---|---|---|---|
| 2026-10-24 | 156.0.8078.48 | 156.0.8078.70 | 1 min | 70 min | 72 min | hooks applied as they were |
| 2026-10-26 | 156.0.8078.70 | 157.0.8103.23 | 3 min | 153 min | 167 min | hooks moved by hand: H0 H1 H3 H13 H14 H15 H17 H18; built at the first try; Cromarc's fixes only in `src/hooks/` (H15 takes Chrome's new window set, H18 no longer touches the top container); adapters: only their nogncheck comments; features: none |
