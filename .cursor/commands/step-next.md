# Step: next

Orient on the next PLAN step without doing any work. **Read-only:** do not implement, edit code, flip step status, or commit.

## Which step

- Default: the **first step not marked `done`** in `PLAN.md`.
- If the user names a step (e.g. `/step-next 2`), summarize that one.

Read `PLAN.md` (and `goal.md` if needed) before answering.

## Report

Short, scannable:

- **Step & status** — title, status (`todo` / `in progress` / `done`)
- **Goal** — one or two sentences
- **Depends on** — prior steps; flag any not `done`
- **TDD?** — host red-first vs device/characterization
- **Deliverables** — files/components expected
- **Out of scope** — v1 locks that must not creep in
- **Suggested next** — `/step-go` after user agrees

Remind: orientation only — does not start the step.
