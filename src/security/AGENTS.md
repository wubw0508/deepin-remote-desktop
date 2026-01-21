# PROJECT KNOWLEDGE BASE

## OVERVIEW
security handles TLS credentials, NLA SAM generation, and PAM authentication.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| TLS credentials | `src/security/drd_tls_credentials.c` | load/apply/reload PEM |
| NLA SAM | `src/security/drd_nla_sam.c` | temp SAM file creation |
| PAM auth | `src/security/drd_pam_auth.c` | TLS-only login |

## CONVENTIONS
- SAM files are temporary and deleted after use.
- Handover uses PEM strings, not shared files.

## ANTI-PATTERNS
- Do not log secrets or raw credentials.
- Do not persist SAM files or TLS keys in source control.
