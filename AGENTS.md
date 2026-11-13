# AGENTS.md

Guidance for AI coding agents working in **keepsake**.

## Project overview

A text-based RPG where your AT Protocol DID is the save file

- Language: C++
- Default branch: main

## Wolfram is the protocol layer

Wolfram is Ewan's C AT Protocol SDK and is the intended protocol implementation for Keepsake. It is fetched via CMake FetchContent and linked when `KEEPSAKE_WITH_WOLFRAM=ON` (the default).

When Wolfram is enabled, it is a hard dependency. If the fetch or build fails, the build fails — there is no silent fallback. Do not set `KEEPSAKE_WITH_WOLFRAM=OFF` unless the entire protocol layer is being intentionally excluded.

## Build numbers and version stamping

Every Keepsake build carries stamped values from CMake. A release must never ship without build identifiers present. If commit SHAs are not being recorded in CI, the 6-character short SHA is the fallback identifier.

## Working rules

- Inspect the README, manifests, CI workflows, and nearby code before editing.
- Preserve existing architecture, naming, formatting, and error-handling conventions.
- Use project scripts for validation; never claim checks you did not run.
- Keep changes scoped and update tests or documentation when behavior changes.
- Use feature branches and pull requests.
- Treat generated files, credentials, deployment configuration, and release metadata as sensitive.

## Recent history

chore(deps): pin wolfram to v0.41.0; docs: add Wolfram and build number policy to AGENTS.md; docs: document pinned wolfram dependency; build(wolfram): fetch SDK pinned to v0.22.0 via FetchContent; docs: tailor contributor and agent guidance (#2); Update issue template: config.yml; Update issue template: documentation.yml

<!-- allowed-agents -->
## Allowed agents

Only **Claude** (Anthropic) and **Mistral** (Mistral AI) may be used as coding agents in this repository. This rule supersedes any earlier agent-attribution guidance in this file.

- Agent-assisted commits must credit only one of those agents:
  - `Co-Authored-By: Claude <noreply@anthropic.com>`
  - `Co-Authored-By: Mistral Vibe <vibe@mistral.ai>`
- Allowed agent footers are `🤖 Generated with [Claude Code](https://claude.com/claude-code)` and `🤖 Generated with [Mistral Vibe](https://mistral.ai)`.
- Do not add `Co-Authored-By:` trailers, `Generated with` footers, author names, or committer identities for any other agent or AI tool.
- Do not credit another agent in commit metadata.
