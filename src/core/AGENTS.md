# PROJECT KNOWLEDGE BASE

## OVERVIEW
core owns application startup, config merge, runtime aggregation, and encoding option plumbing.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| App lifecycle | `src/core/drd_application.c` | main loop, signal handling |
| Config parsing | `src/core/drd_config.c` | INI + CLI merge |
| Runtime glue | `src/core/drd_server_runtime.c` | capture/encoding/input aggregation |
| Encoding options | `src/core/drd_encoding_options.h` | shared encoding config |

## CONVENTIONS
- Config merge order is INI then CLI override.
- Runtime mode is an enum; update callers if the enum changes.

## ANTI-PATTERNS
- Do not bypass `drd_server_runtime_set_transport()` when switching frame transport.
- Do not add new config keys without updating defaults in `data/config.d/`.
