# PROJECT KNOWLEDGE BASE

## OVERVIEW
encoding manages RFX/Progressive/H264 encoding, dirty-rect detection, and codec switching.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Encoding manager | `src/encoding/drd_encoding_manager.c` | codec selection, dirty rects |

## CONVENTIONS
- Progressive path uses RLGR1 for compatibility.
- SurfaceBits is the fallback path when rdpgfx is unavailable.

## ANTI-PATTERNS
- Do not bypass dirty-rect collection when diff is enabled.
- Do not change codec switching without aligning session fallback logic.
