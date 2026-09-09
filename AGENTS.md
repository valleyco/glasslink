# Agent notes — esp32-wl-display

Living product/process state: **[`PLAN.md`](PLAN.md)**. Intent: [`goal.md`](goal.md).

## Cursor pack

| Path | Role |
|------|------|
| [`.cursor/rules/global-contract.mdc`](.cursor/rules/global-contract.mdc) | Always-on invariants (TDD, v1 lock, layers) |
| [`.cursor/commands/`](.cursor/commands/) | `/step-next` `/step-go` `/step-done` `/plan-decide` `/commit` |
| [`.cursor/skills/wl-display/`](.cursor/skills/wl-display/SKILL.md) | Architecture + decision cheat sheet |

## Defaults

- Do not start a PLAN step without user go.
- Host tests before device; no IDF in `codec` / `contract` / `render`.
- v1: L0 + clear, binary MQTT envelope, inline only, no touch/TLS/HTTP.
- Commit only when asked; no secrets in git.

If your tool does not load `.cursor/`, still follow this file and `PLAN.md`.
