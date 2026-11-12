# Instructions for agents contributing to RAS Daemon

These instructions apply throughout this repository. Read
`docs/contributing.rst` and `docs/development.rst` before changing code.
Follow `.editorconfig` and the conventions of the files being edited.

## Human submission and review

RAS Daemon accepts contributions submitted by humans. Contributions submitted
autonomously by LLM agents or bots are not accepted. Using an LLM to prepare
code, documentation, tests, or a patch is allowed, provided a human reviews the
contribution before submission.

- Prepare changes for a human to inspect. The submitting human must understand
  and review the complete patch, including generated tests and documentation,
  and take responsibility for its correctness and provenance.
- Do not autonomously submit a PR or send patches or review comments as an
  agent. Leave submission to the human contributor after their review.
- Each submitted patch needs a `Signed-off-by:` trailer using the human
  contributor's real name, certifying the Developer Certificate of Origin 1.1.
  Never invent a human identity, sign-off, review, or approval. An agent cannot
  certify the DCO on a human's behalf.
- Automated checks and another LLM's review do not replace human review.

## Contribution workflow

- Search existing issues and PRs before preparing a contribution. Reference
  related reports and explain how the change addresses them.
- Submit human-reviewed PRs against `https://github.com/mchehab/rasdaemon/`.
  GitLab and infradead are mirrors; patches are no longer tracked through the
  Linux EDAC mailing list.
- Keep each patch focused on one logical change. Avoid unrelated formatting,
  refactoring, generated files, and changes to release versions or changelogs.
  Preserve existing work in the checkout.
- Use a short subject naming the affected component, such as
  `core/trigger: release allocated names on failure`. Explain the problem,
  relevant trigger, and reason for the fix in the commit body. Include a
  `Fixes:` trailer identifying the introducing commit when established.
- Base fixes on evidence from the current source, a reproducer, or an applicable
  specification. Static-analysis reports need investigation; silence a warning
  by fixing its cause, not by hiding it or adding meaningless checks.
- Report what was built and tested, with configurations, results, and any
  limitations. Never claim to have run a check that was not run.

## Coding style

- Follow Linux kernel C coding style and submission conventions. Use tabs with
  eight-column stops for C, headers, shell, and Makefiles; follow the existing
  continuation alignment. Put function opening braces on their own line and
  control-statement opening braces on the statement line.
- Use descriptive `snake_case` identifiers and uppercase macro names. Prefer
  small functions, straightforward control flow, and existing helpers over new
  abstractions. Keep declarations and error handling consistent with nearby
  code. Avoid unnecessary casts and typedefs.
- Follow `.editorconfig`: Python and Meson use four spaces; YAML uses two.
  Use UTF-8, LF endings, a final newline, and no trailing whitespace.
- Preserve SPDX license identifiers and the licensing of existing files.
  Document public and internal C interfaces with Linux kernel-doc notation;
  add public APIs to the API documentation as described in the developer guide.
- Explain non-obvious hardware or protocol semantics in comments. Keep register
  layouts, bit masks, endianness, and decoder behavior grounded in the relevant
  specification. Do not change decoder output casually: users and reporting
  tools depend on it.

## Correctness and architecture

- Validate external trace data and vendor payload lengths before reading fields
  or casting buffers. Check allocation, parsing, and I/O results. Handle integer
  overflow, truncation, signedness, and shift widths explicitly; use types wide
  enough for hardware register values and database fields.
- Initialize state before use. Release allocations, file descriptors, and
  statements on success and every failure path. Preserve useful error codes
  and report actionable errors without treating absent optional hardware as a
  fatal failure.
- Include the headers that declare the interfaces used. Do not depend on
  transitive glibc includes; preserve builds with musl and other supported
  environments. Keep architecture-specific behavior within its feature and
  architecture selection.
- Use the existing self-registering module, event, and consumer interfaces.
  Descriptors have static lifetime. Modules initialize in dependency order and
  clean up in reverse order; successful initialization must leave owned state
  reachable through its module context.
- Optional modules communicate through registration and published events, not
  direct calls across optional module boundaries. Published event payloads are
  borrowed synchronously; consumers must not retain them after returning.
- Event modules own their registered database descriptors and statement pairs.
  Keep them valid while registered and unregister after statements are no
  longer in use. Use `db/ras-db.h` and its no-database implementations instead
  of spreading database feature conditionals through event code.
- Integrate features into the appropriate Meson lists and document new runtime
  options and dependencies. Preserve normal operation when optional features
  are disabled and compatibility with existing databases and configurations.

## Tests and validation

Agents are expected to write or update unit tests when applicable to prevent
future regressions in changed code. For a bug fix, cover the failing input or
behavior and meaningful boundaries or failure paths. Tests should demonstrate
the bug before the fix where practical and exercise externally meaningful
behavior rather than merely reproduce implementation details. Do not weaken
existing tests to make a patch pass.

Follow the test registration contracts in `docs/development.rst`. Attach event
tests to their event descriptors; use standalone registration for other suites.
Keep default tests hermetic. Use integration or VM tests when the behavior
depends on real kernel interfaces, hardware, or databases, and explain what a
unit test cannot establish.

Before submission:

1. Build the affected configuration and run the hermetic suite:

   ```sh
   meson setup build
   ninja -C build
   meson test -C build --print-errorlogs
   ```

   For an existing build, use its configured options or a separate build
   directory. Do not overwrite a contributor's build configuration. Build with
   both GCC and Clang when available; fix compiler warnings. Check relevant
   feature-disabled and architecture-selected configurations.

2. For Python changes, run `make python-test`. MySQL/MariaDB and PostgreSQL
   integration tests require provisioned `rasdaemon_test` databases; consult
   `.github/workflows/unittest.yml`. Do not report those checks as passing if
   their services were unavailable.

3. Check every patch with the project's script:

   ```sh
   scripts/checkpatch.pl --strict --no-tree PATCH
   git diff --check
   ```

   CI also checks tracked C and header files using
   `scripts/checkpatch.pl --terse --show-types --no-summary -f` and filters the
   existing exceptions in `.github/workflows/checkpatch_ignore.txt`. Fix new
   findings; do not expand the ignore list merely to get a passing result.

4. Check affected documentation. The Sphinx build treats warnings as errors;
   resolve documentation and kernel-doc warnings before submission.

## Security and project scope

Follow `docs/security.rst` for suspected vulnerabilities, including its private
reporting route. Do not publish vulnerability details through ordinary issues
or PRs before the appropriate review. Human contributors retain control of
external communication.

Contributions are upstream project work. Do not make support, warranty,
product-security, or regulatory commitments on behalf of the project, its
maintainers, or other contributors. Follow the contributor-role boundaries in
`docs/contributing.rst`.
