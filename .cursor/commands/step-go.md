# Step: go

Start the **next agreed PLAN step** (or the step the user names).

## Before coding

1. Read `PLAN.md` — confirm the step is not `done` and dependencies are satisfied.
2. Read skill `wl-display` and `.cursor/rules/global-contract.mdc` invariants.
3. Set step status to `in progress` in `PLAN.md` if it was `todo`.
4. If the step is **TDD** (`render` / `codec` / `contract`):
   - Write **failing** host tests first
   - Implement until the relevant `make test-*` is green

## While working

- Stay inside step scope — no HTTP, CBOR, L1, touch, TLS unless the step says so.
- Match project style: ESP-IDF later, host gcc tests now, pure C components, minimal diff.
- HAL: shared `hal_display.h` + link backend; no vtable.
- Prefer invaders display patterns when copying SPI code; strip game/touch.

## When finished

1. Run host tests for touched suites; `make build` if firmware changed (when skeleton exists).
2. Update `PLAN.md` — step → `done` with date and one-line note (or leave `in progress` and summarize if blocked).
3. Tell the user what landed; suggest `/step-done` if still open, or `/commit` if they want a commit.

Do **not** commit unless the user asks.
