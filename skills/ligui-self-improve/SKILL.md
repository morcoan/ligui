---
name: ligui-self-improve
description: Turn a successful non-trivial approach or user correction into a reusable global skill without duplicating existing skills.
whenToUse: After a novel multi-step task, a user correction, or when the same workflow is likely to recur across projects.
metadata:
  version: 1
  scope: global
---

# Ligui self-improve

## When to write a skill

Write one when:

- a task took five or more steps and is likely to recur;
- the user corrected your approach;
- you discovered a non-obvious command, API, or failure mode;
- an existing skill was wrong or incomplete.

Do not write a skill for one-off facts; put those in memory instead.

## Where

`~/.dsh/skills/<kebab-name>/SKILL.md` is global and is discovered in every
project. Write it with the normal file tools; the skill catalog refreshes
automatically.

## Template

    ---
    name: kebab-name
    description: One sentence explaining what this skill does and when to use it.
    whenToUse: The trigger conditions in one or two sentences.
    metadata:
      version: 1
      created: 2026-10-30
      tags: [tag1, tag2]
    ---

    # Title

    ## When to use

    ## Prerequisites

    ## Steps

    1. ...

    ## Verify

    ## Pitfalls

    ## Examples

## Rules

- Search the current skill catalog first. Update the existing skill instead of
  adding a near-duplicate.
- Keep `SKILL.md` under about 200 lines. Put long scripts in `scripts/` and
  reference material in `references/` beside it.
- Include a verification step and at least one concrete example.
- After writing, call the `skill` tool with the exact name to confirm it loads.
  Fix the frontmatter if it does not.
- Record the skill in memory as `Workflow:<name>` with a one-line description.
