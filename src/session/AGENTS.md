# PROJECT KNOWLEDGE BASE

## OVERVIEW
session manages FreeRDP peer lifecycle, session state, and rdpgfx pipeline/backpressure.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Session lifecycle | `src/session/drd_rdp_session.c` | post_connect/activate/disconnect |
| Rdpgfx pipeline | `src/session/drd_rdp_graphics_pipeline.c` | ACK capacity control |

## CONVENTIONS
- Render thread performs wait -> encode -> submit; keep that order intact.
- Backpressure uses `capacity_cond` and `outstanding_frames` in the pipeline.

## ANTI-PATTERNS
- Do not skip ACK capacity checks when submitting frames.
- Do not start render thread before session activation succeeds.
