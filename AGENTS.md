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

Sync AGENTS.md from zincfox; Sync AGENTS.md from zincfox; Sync AGENTS.md from zincfox; Sync CONTRIBUTING.md from zincfox; Sync CONTRIBUTING.md from zincfox