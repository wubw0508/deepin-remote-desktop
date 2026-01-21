# PROJECT KNOWLEDGE BASE

## OVERVIEW
capture provides X11 capture, XDamage handling, and frame queue production.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Capture manager | `src/capture/drd_capture_manager.c` | queue + lifecycle |
| X11 capture | `src/capture/drd_x11_capture.c` | XDamage/XShm loop |

## CONVENTIONS
- Capture loop is fixed-interval; XDamage is used to merge regions.
- Frame queue capacity is 3 frames.

## ANTI-PATTERNS
- Do not rely on XDamage frequency for pacing.
- Do not increase queue capacity without updating metrics expectations.
