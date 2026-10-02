# Script fuzzing

Shaders and scripts come from untrusted content, so the compiler is a security boundary: no input may crash it,
corrupt memory, hang it or make it produce something inconsistent. These fuzz targets check that with libFuzzer,
AddressSanitizer and UndefinedBehaviorSanitizer.

## Targets

- **FuzzScript** (`Fuzz_Script.cpp`): raw source text. libFuzzer's byte mutations with `Script.dict`, plus a
  token-level mutator that moves, repeats, swaps and brackets whole tokens, so mutated inputs get past the parser more
  often. Seeds in `Seeds/Script`.
- **FuzzScriptGrammar** (`Fuzz_ScriptGrammar.cpp`): generated programs. The input is read as a stream of decisions for
  a generator that knows the grammar and type rules, so coverage guidance works on programs that parse and type.
  - *Valid mode* generates only programs the compiler accepts today and computes, independently, every const's value,
    every declaration's type, every struct's layout and every entry point's inputs and outputs. An error or any
    difference fails. It has to follow language changes.
  - *Chaos mode* (odd first byte) mixes in type errors, unsupported constructs, nesting around the recursion limit,
    bad attributes, cyclic structs, huge arrays, exponentially growing constants and broken shader interfaces
    (missing, duplicate or misplaced semantics and locations, nested entry points).

## What's checked for every input (`FuzzCommon.cpp`)

- Crashes, memory errors (ASan) and undefined behavior (UBSan). Arenas are poisoned under ASan, and fuzz builds leave a
  redzone before each arena allocation, so overflows inside an arena are caught too.
- Panics are failures: untrusted input mustn't be able to take the engine down.
- Memory: a compile's arenas may use 64 MB plus 1 KB per source byte. More means a small input made the compiler use
  disproportionate memory, e.g. constants built from repeated references to other constants.
- A stage fails if and only if it reports an error. Errors live in the output arena, survive the intermediate arena
  being thrown away, and are printable ASCII (they end up in logs and terminals).
- The tree is well formed after every stage: each node type has the children it should, nothing is shared or cyclic.
  After resolving: no identifiers left, scopes set. After typing: everything has a type, every const a value of the
  right size, layouts are consistent, array types unique, attributes are intrinsics. Every `@` in the source is one
  attribute in the tree, so none get dropped.
- After the shader interface pass: the entry points are exactly the top-level `@entry` functions, in order. Every
  input and output's path leads to its declaration and leaf type, that declaration has exactly the recorded semantic
  or location, every leaf of the parameters and return value is covered once, semantics match their stage, direction
  and type, nothing is used twice, and vertex shaders output `position`.
- Backends: every program that gets through the front end has its indices clamped, the IR validated again, and each
  entry point emitted as SPIR-V and HLSL. The output has to pass SPIRV-Tools' validator (from the Vulkan SDK, when CMake
  finds it) and compile with fxc (`D3DCompile`, Windows), on one of the two compiles of each input.
- Determinism: the shader is compiled twice with arenas filled with different garbage, and the results must match,
  which catches reads of uninitialized arena memory.
- The source is also compiled as a script, which has a different global scope.

A failed check prints the source and aborts, so libFuzzer saves the input like any crash.

## Building and running

The `Fuzz` preset instruments everything (clang only, RelWithDebInfo, static CRT to match LLVM's prebuilt libFuzzer
on Windows):

```
cmake --preset Fuzz
cmake --build Build/Fuzz
```

Fuzz, keeping the corpus under `Build/`:

```
Build/Fuzz/FuzzScript Build/Fuzz/Corpus/Script EVA/Script/Fuzz/Seeds/Script -dict=EVA/Script/Fuzz/Script.dict -artifact_prefix=Build/Fuzz/Crashes/
Build/Fuzz/FuzzScriptGrammar Build/Fuzz/Corpus/ScriptGrammar -artifact_prefix=Build/Fuzz/Crashes/
```

Useful flags: `-max_total_time=<seconds>`, `-jobs=N -workers=N` for parallel runs, `-fork=N -ignore_crashes=1` to keep
going past crashes and collect several, `-runs=0 <dir>` to run a corpus once.

An 8 hour run of a target on 8 cores (`-fork=8`), collecting every crash rather than stopping at the first:

```
Build/Fuzz/FuzzScript Build/Fuzz/Corpus/Script EVA/Script/Fuzz/Seeds/Script -dict=EVA/Script/Fuzz/Script.dict -fork=8 -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 -max_total_time=28800 -artifact_prefix=Build/Fuzz/Crashes/Script/ > Build/Fuzz/script.log 2>&1
Build/Fuzz/FuzzScriptGrammar Build/Fuzz/Corpus/ScriptGrammar -fork=8 -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 -max_total_time=28800 -artifact_prefix=Build/Fuzz/Crashes/ScriptGrammar/ > Build/Fuzz/grammar.log 2>&1
```

The `Crashes/<target>` directories have to exist. Progress lines in the logs end with `oom/timeout/crash` counts.

## Coverage

To see which code the fuzzers reach, replay the seeds, regressions and `Build/Fuzz/Corpus` through a build instrumented
for coverage (the `Coverage` preset, also clang only):

```
cmake --preset Coverage
cmake --build Build/Coverage --target CoverageReport
```

It prints a summary per file and writes a line by line HTML report to `Build/Coverage/coverage/html/index.html`. Code
the corpora never reach is what to write seeds, dictionary entries or generator rules for.

## A crash

Run the target on the saved input to reproduce it. A failed check prints the source; for the grammar target,
`EVA_FUZZ_PRINT=1` prints the generated program even when something crashes before a check. `-minimize_crash=1
-runs=10000 <input>` shrinks it.

Once fixed, add a test for the bug, and put the input under `Regressions/<target>/` (for the grammar target the input
file itself, it isn't source).

## Regressions in normal builds

Without `EVA_FUZZ` the targets are built with `ReplayMain.cpp` instead of libFuzzer and registered with CTest, running
over `Seeds/<target>` and `Regressions/<target>`. The checks still run, just without sanitizers.
