# PROJECT KNOWLEDGE BASE

**Generated:** 2026-01-21 10:18:10 CST
**Commit:** 02e245c
**Branch:** master

## OVERVIEW
deepin-remote-desktop is a C17 + GLib/GObject + FreeRDP 3.x RDP server with three runtime modes (user/system/handover) and DBus-driven handover.

## STRUCTURE
```
./
├── src/                    # C17 core sources (single executable)
│   ├── core/               # config, application, runtime glue
│   ├── session/            # RDP session state machine, graphics pipeline
│   ├── transport/          # listener, routing token, TLS/NLA setup
│   ├── security/           # TLS credentials, PAM, NLA SAM
│   ├── system/             # system/handover daemons
│   ├── capture/            # X11 capture
│   ├── encoding/           # RFX/H264 encoding
│   ├── input/              # X11 input injection
│   └── utils/              # frames, queues, logging, metrics
├── data/                   # config templates, certs, systemd units, DBus policy
├── doc/                    # architecture, changelog, task logs
├── debian/                 # packaging rules
└── build/                  # meson output (ignored)
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| App entry + CLI | `src/main.c`, `src/core/drd_application.c` | bootstraps GLib main loop |
| Config merge | `src/core/drd_config.c` | INI + CLI merge, encoding options |
| Session state | `src/session/drd_rdp_session.c` | render thread, lifecycle |
| GFX pipeline | `src/session/drd_rdp_graphics_pipeline.c` | Rdpgfx ACK/backpressure |
| Listener | `src/transport/drd_rdp_listener.c` | TLS/NLA, mode branching |
| Handover | `src/system/drd_system_daemon.c`, `src/system/drd_handover_daemon.c` | DBus dispatcher |
| Encoding | `src/encoding/drd_encoding_manager.c` | RFX/H264, dirty rects |
| X11 capture | `src/capture/drd_x11_capture.c` | XDamage/XShm |
| Input | `src/input/drd_x11_input.c` | XTest injection |
| TLS/PAM/NLA | `src/security/` | credential setup |

## CONVENTIONS
- Single executable build: all module sources compile into `deepin-remote-desktop` (no intermediate static libs).
- Logs must be in English (`g_message`, `g_warning`); inline comments are in Chinese.
- Format edited C regions with `clang-format -style=LLVM` (120 column limit).

## ANTI-PATTERNS (THIS PROJECT)
- Do not store personal keys in `data/certs/`.
- Do not commit scratch files outside `buildDir/`.
- Do not edit `doc/TODO.md` unless explicitly requested.

## UNIQUE STYLES
- DBus handover workflow (system/handover daemons + routing token).
- GObject-based modules with shared utilities in `src/utils/`.

## COMMANDS
```bash
meson setup build --prefix=/usr --buildtype=debugoptimized
meson compile -C build
meson test -C build --suite unit
./build/src/deepin-remote-desktop --config ./data/config.d/default-user.ini
```

## NOTES
- Runtime modes: user (desktop share), system (remote login), handover (connection take-over).
- DBus interfaces are generated via `gnome.gdbus_codegen()` from `src/*.xml`.
