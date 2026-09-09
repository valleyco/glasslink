# Plan: decide

Record a new or updated decision in `PLAN.md` (W-table or step notes). **Discuss first** — do not flip `agreed` without user confirmation.

## When to use

- Scope change (L1 early, HTTP phase, envelope format)
- Board/profile change
- TDD/process change
- Reordering or splitting PLAN steps
- Compression keep/kill after benches

## Workflow

1. Read current `PLAN.md` decisions (W1–W13) and open steps.
2. Propose the change: what, why, impact on steps/backlog.
3. **Wait for user yes** before editing (unless user already stated the decision in-thread).
4. Update `PLAN.md`:
   - Add or edit a **W-row** with status `agreed` (or `discuss` if still open)
   - Adjust affected step text if scope changed
   - If reversing an `agreed` decision, note why briefly
5. If the decision affects the always-on contract, update `.cursor/rules/global-contract.mdc` and `docs/architecture-draft.md` in the same change when practical.
6. Summarize what changed and which step is next.

## Do not

- Start implementation in the same turn unless the user also said `/step-go` or “implement”.
- Mark steps `done` here.
- Commit unless the user asks `/commit`.
