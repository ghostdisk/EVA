# Security model

EVA runs content from untrusted sources. A game is downloaded and run like a web page, so everything that comes with it
has to be treated as hostile:

- **Untrusted code:** scripts and shaders, compiled and run by the engine.
- **Untrusted assets:** images, fonts, audio, video, models and other data, decoded by the engine and by third party
  libraries (libpng, libavif, FreeType and others).

## What we protect, in order

1. **The engine process.** Content must not be able to corrupt memory, take control of execution, or read data it
   wasn't given. A bug that allows any of this is critical.
2. **The GPU driver.** Drivers and their shader compilers aren't hardened against hostile input, so everything EVA hands
   them has to be valid and safe by construction.
3. **Availability.** Content shouldn't be able to crash or hang the engine, but this ranks well below the first two.
   Content that exhausts memory and ends the process is accepted, much like a web page that does so gets its renderer
   killed. Small inputs that cost disproportionate memory or time are still bugs.

Out of scope for now: timing and other side channels, and bugs in drivers or the OS triggered by valid input.

## What we do

- **Untrusted input is a boundary in the design.** Code that handles it validates everything, enforces limits on sizes
  and nesting, and reports bad input as an error instead of trusting it.
- **Testing.** Every subsystem that handles untrusted input has unit tests, including malformed and hostile inputs.
- **Fuzzing.** Coverage-guided fuzzing with AddressSanitizer and UndefinedBehaviorSanitizer, checking crashes, internal
consistency, determinism, and disproportionate resource use. Inputs behind fixed bugs are kept and rerun with the tests.
Coverage reports show what the fuzzers don't reach, so it can be targeted.

## What we plan

- **Process sandboxing.** Compiling untrusted code and decoding untrusted assets will happen in separate low-privilege
  processes, so a bug there can't compromise the engine. This includes the third party asset libraries, which are
  outside our control.
- **Safe GPU code.** Code generated from untrusted shaders is made safe before it reaches a driver: accesses are kept in
  bounds and behavior is defined. The output is validated, and GPU robustness features are used where available.
- **Hardened builds:** compiler hardening options, static analysis, and warnings as errors.
- **Keeping third party code current** and following its security fixes.

## Current status

EVA is in early development. Of the subsystems above, the script and shader compiler exists, is unit tested and is
fuzzed continuously. Process sandboxing, GPU code generation and its protections, and the third party asset libraries
aren't in place yet. Until they are, untrusted content shouldn't be run outside of development.

## Reporting

See [SECURITY.md](../SECURITY.md).
