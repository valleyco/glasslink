# Commit

Create a git commit for the current changes (typically one PLAN step).

## Style

- **Imperative summary**, optional 1–2 sentence body focusing on *why*.
- Step completion: mention the step, e.g. "Step 1: host fake_display tests green."
- Match recent history when git log exists (`git log --oneline -5`).
- Include `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>` trailer when committing via agent (project norm for Copilot CLI).

## Steps

1. Run `git status`, `git diff`, and `git diff --staged` in parallel (init repo only if user asked).
2. Run `git log --oneline -5` for tone when history exists.
3. Draft message; do **not** stage secrets (`sdkconfig` with passwords, `sdkconfig.local`, `.env`, credentials).
4. Stage relevant files only.
5. Commit with HEREDOC:

   ```bash
   git commit -m "$(cat <<'EOF'
   <summary>

   <body>

   Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>
   EOF
   )"
   ```

6. `git status` to confirm.

## Rules

- Only commit; do **not** push unless asked.
- Never update git config; no `--amend` unless asked; no hook skips.
- No empty commit if nothing changed.
- If the repo is not a git repo yet, ask before `git init`.
