---
name: pre-commit-review
description: The mandatory review before every commit to White Wolf Engine. Use when about to commit or when asked to review a change - it covers formatting, verification on every preset, the diff review checklist and the commit message.
---

# Pre-commit review

Every commit goes through these steps in order. If any step fails, fix the problem and start again from step 1.

## 1. Format

```
cmake -P Tools/Format.cmake
```

This rewrites all first-party C++ files with clang-format 18. If it reports that clang-format 18 is unavailable, install it; do not commit unformatted or differently formatted code.

## 2. Verify

Run every workflow preset this machine supports (see the `build-and-test` skill):

- Windows: `cmake --workflow --preset windows-debug` and `cmake --workflow --preset windows-release`
- Linux: `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-sanitize`

All must end with `100% tests passed`, with no warnings and no avoidable skipped tests.

## 3. Review the diff

Stage the change, then read `git diff --staged` line by line. Confirm each point; fix anything that is not true.

**Code style** (rules in `AGENTS.md`, "Code style")

- [ ] Types, functions and variables are `PascalCase` with the right prefix (`W`, `F`, `E`, `I`, `T`, `b`).
- [ ] One type per header/source pair; file named after the type without its prefix.
- [ ] `Out`/`In` parameter prefixes are used correctly; nothing is shadowed.
- [ ] No `auto` outside lambdas, iterators and unspellable types; no C-style casts; no raw `new`/`delete`.
- [ ] Public types and functions have `/** ... */` documentation that states the contract.
- [ ] Includes are minimal and direct; own header first; `CoreMinimal.h` first in headers.

**Production quality**

- [ ] No `TODO`, `FIXME`, hack, placeholder, dead code, commented-out code or debugging leftover.
- [ ] Every error path is handled and reported; nothing fails silently.
- [ ] No warning was disabled and no test was weakened to make the build pass.
- [ ] No undefined behaviour, no reliance on unspecified ordering or platform-specific sizes.
- [ ] Nothing outside the repository was read or copied.

**Tests**

- [ ] Every new or changed behaviour has unit tests in `<Module>/Tests/`, including failure paths and boundaries.
- [ ] The tests fail without the change (they test the behaviour, not the implementation).
- [ ] The full suite passes on every preset from step 2.

**Exports and build**

- [ ] Symbols used across modules carry the module API macro; nothing else does.
- [ ] New third-party code follows the `add-third-party-dependency` skill.

**Documentation**

- [ ] `AGENTS.md`, `README.md`, `docs/DECISIONS.md` and the skills are still accurate. Decisions made in this change are recorded in `docs/DECISIONS.md`.

**Scope**

- [ ] The commit is one logical change. Unrelated edits are split into their own commits.

## 4. Commit

```
git commit
```

- Subject: imperative mood, at most 72 characters, no trailing period. Example: `Add strict version parsing`.
- Body: what changed and why, wrapped at 72 columns. Mention which presets were verified when MSVC was not among them.
- Never use `--no-verify`, and never amend or force-push commits that are already on `origin/main`.

## 5. After pushing

Check that the CI run for the commit is green. A red `Windows MSVC` job means the build is broken on the product toolchain: fix it before any other work.
