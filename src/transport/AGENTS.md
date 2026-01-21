# PROJECT KNOWLEDGE BASE

## OVERVIEW
transport owns the RDP listener, routing token parsing, and TLS/NLA settings injection.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Listener lifecycle | `src/transport/drd_rdp_listener.c` | GSocketService, peer setup |
| Routing token | `src/transport/drd_rdp_routing_token.c` | MSG_PEEK parse |

## CONVENTIONS
- Routing token format is `Cookie: msts=<token>`.
- Delegate hook is used for system/handover flow.

## ANTI-PATTERNS
- Do not change routing token format without updating system/handover logic.
- Do not bypass delegate handling in system mode.
