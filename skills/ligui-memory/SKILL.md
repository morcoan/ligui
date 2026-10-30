---
name: ligui-memory
description: Maintain Ligui's persistent cross-project memory graph and cheat-sheet, and recall user preferences before acting.
whenToUse: The user says remember, corrects you, states a preference, or you discover a command or workflow likely to recur.
metadata:
  version: 1
  scope: global
---

# Ligui memory

The memory graph is a local JSONL file at `~/.dsh/ligui-memory.jsonl`, exposed
through the `mcp__memory__*` tools. It is shared by every project.

## Entity schema

- `User` (`person`) — one entity. Observations: name or handle, language,
  timezone, communication style, tool preferences, constraints, recurring goals.
- `Project:<name>` (`project`) — stack, conventions, build/test commands,
  gotchas, repo path.
- `Workflow:<name>` (`workflow`) — a reusable sequence of steps.
- `Command:<name>` (`command`) — a verified command or snippet.
- `Lesson:<topic>` (`lesson`) — a correction or failure mode.

Use relations for `User -> prefers -> Workflow`, `Project -> uses -> Command`,
and `Lesson -> applies_to -> Project`.

## Before a task

1. `mcp__memory__search_nodes` with the user, the project name, and two or
   three task keywords.
2. `mcp__memory__open_nodes` for the best matches.
3. Apply relevant preferences. If two memories conflict, say so instead of
   guessing.

## After learning

- Preference or correction: `mcp__memory__add_observations` on `User`
  (create the entity first if it does not exist).
- Project fact: `mcp__memory__create_entities` or `add_observations` on
  `Project:<name>`.
- Reusable approach: save a skill (see `ligui-self-improve`) and record
  `Workflow:<name>`.
- Search before writing. Merge near-duplicates instead of appending them.
- Never store passwords, API keys, tokens, private keys, or full credentials.

## Style

- One fact per observation, short and testable.
- Include the date or project when it matters, for example
  `2026-10-30: prefers pnpm over npm`.
- Mark stale facts with `superseded:` rather than deleting history.
