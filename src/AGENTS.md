# PROJECT KNOWLEDGE BASE

## OVERVIEW
`src/` hosts the full C17 codebase for the single `deepin-remote-desktop` executable. Modules are split by runtime concerns (core/session/transport/security/system/capture/encoding/input/utils).

## STRUCTURE
```
src/
├── core/       # application entry, config, runtime glue
├── session/    # RDP session state + rdpgfx pipeline
├── transport/  # listener, routing token, TLS/NLA setup
├── security/   # TLS credentials, NLA SAM, PAM
├── system/     # system/handover daemons, DBus workflow
├── capture/    # X11 capture
├── encoding/   # RFX/H264 encoding
├── input/      # X11 input injection
├── utils/      # frames, queues, logging, metrics
└── main.c      # minimal main entry
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| App entry | `src/main.c`, `src/core/drd_application.c` | GLib main loop bootstrap |
| Config merge | `src/core/drd_config.c` | INI + CLI merge |
| Listener | `src/transport/drd_rdp_listener.c` | TLS/NLA, mode branching |
| Session state | `src/session/drd_rdp_session.c` | render thread lifecycle |
| Rdpgfx | `src/session/drd_rdp_graphics_pipeline.c` | ACK/backpressure |
| Handover | `src/system/drd_system_daemon.c` | DBus dispatcher |
| Encoding | `src/encoding/drd_encoding_manager.c` | RFX/H264 |
| Capture | `src/capture/drd_x11_capture.c` | XDamage/XShm |
| Input | `src/input/drd_x11_input.c` | XTest injection |

## CONVENTIONS
- Keep `src/meson.build` in sync when adding or removing source files.
- GObject-based modules use `G_DECLARE_FINAL_TYPE`/`G_DEFINE_TYPE`.
- Logs in English; inline comments in Chinese.

## ANTI-PATTERNS
- Do not edit generated DBus C files under `buildDir/src/`.
- Do not change routing token formats without updating system/handover flow.
