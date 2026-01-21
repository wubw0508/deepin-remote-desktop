# PROJECT KNOWLEDGE BASE

## OVERVIEW
input injects keyboard/mouse/Unicode events via XTest with scancode translation.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Input dispatcher | `src/input/drd_input_dispatcher.c` | high-level routing |
| X11 injection | `src/input/drd_x11_input.c` | XTest events |

## CONVENTIONS
- RDP scancodes are 8-bit; extended flag is passed separately.
- Coordinates are scaled from encoded to real desktop size.

## ANTI-PATTERNS
- Do not pass 9-bit scancodes directly to FreeRDP keycode mapper.
- Do not skip `update_desktop_size()` when resolution changes.
