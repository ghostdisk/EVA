# TODO

## Script

- `x.5` lexes as `x` followed by the number `.5` rather than a member access, since a `.` followed by a digit always starts a number.
- `a++.b` and `a++++` are accepted: a postfix operator can be followed by more postfix operators and member access.
- Support nested comments
- An attribute's expression swallows a following parenthesis or prefix operator: `@a (x)` parses as the call `a(x)` and `@a -x` as `a - x`. `@a (@b x)`, the example in ParseExpression's comment, parses as `a(@b x)` and then fails on the missing operand.
- A lone `;` (empty statement) is an error, so is a `;` after a statement ending with a block, e.g. `if a {};` (unlike `const`, where it's optional).
- Decide where attributes are allowed, likely declarations only. Today any expression can have them, and on a constant expression they stay on the folded `CONSTANT` node but are ignored.
- Cap shader source size to a few MB, since shaders come from untrusted content.
- Cap the errors per compile and report only the first N, so a large source can't produce an unbounded list.
- `std::vector` growth in the compiler (`Parser`, `Resolver` and `Typer` errors, the expression parser's stacks, `Context::array_types`) throws `std::bad_alloc` when out of memory, which ends the process. Decide whether `std::vector` stays allowed; arena-backed lists would make running out a limit error like the rest.

## Core

- The atom table stores its strings in `std::string`s keyed by an `std::unordered_map`. Replace both: keep the strings in an arena owned by the table so they never move, and use our own hash map keyed by `StringView`. That removes the `std::string` built on every `GetAtom` lookup, and lets atom strings be returned as views without copying them into the caller's arena (e.g. in `SerializeNode`).
- `GetAtom` isn't thread-safe: the table is a global with no locking. Fine while everything is single threaded, but compiling shaders on worker threads needs the new table to support concurrent lookups and inserts. Its atoms also never go away, so untrusted identifiers grow it for the lifetime of the process.
