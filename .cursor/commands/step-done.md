# Step: done

Close out a PLAN step the user names (default: the step currently `in progress`).

## Checklist

1. Read `PLAN.md` — confirm which step to close.
2. **Tests / build:**
   - Logic steps: host `make test` / `test-render` / `test-codec` / `test-contract` must pass.
   - Firmware steps: `make build` must pass; note if device flash was not run.
3. Update `PLAN.md`:
   - Status → `done` with date
   - One-line note (what landed, what was deferred)
4. Summarize for the user: deliverables, manual QA left, next step from PLAN.

## Commit

Do **not** commit automatically. If checks pass and PLAN is updated, suggest:

> Ready to commit — run `/commit` if you want one commit for this step.

One commit per completed step is the project norm (once git exists).
