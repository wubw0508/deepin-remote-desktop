# PROJECT KNOWLEDGE BASE

## OVERVIEW
utils provides shared frames, queues, logging, and capture metrics.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Logging | `src/utils/drd_log.c` | structured log writer |
| Frame queue | `src/utils/drd_frame_queue.c` | capacity 3 |
| Frames | `src/utils/drd_frame.c`, `src/utils/drd_encoded_frame.c` | data containers |
| Metrics | `src/utils/drd_capture_metrics.c` | fps + interval |

## CONVENTIONS
- Log output is English; avoid g_printerr() to prevent reentrancy.
- Frame queue is bounded; dropped frames are expected under load.

## ANTI-PATTERNS
- Do not expand queue capacity without updating capture/encoding assumptions.
- Do not bypass log writer with raw stdout/stderr calls.
