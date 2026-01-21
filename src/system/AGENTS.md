# PROJECT KNOWLEDGE BASE

## OVERVIEW
system manages system/handover daemons, DBus dispatcher, and routing token queues.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| System daemon | `src/system/drd_system_daemon.c` | dispatcher + client queue |
| Handover daemon | `src/system/drd_handover_daemon.c` | TakeClient flow |

## CONVENTIONS
- DBus API names must match `src/org.deepin.RemoteDesktop.xml`.
- Pending queue limits and timeouts are enforced here.

## ANTI-PATTERNS
- Do not edit generated DBus C sources in `buildDir/src/`.
- Do not change DBus method names without updating XML and regenerating.
