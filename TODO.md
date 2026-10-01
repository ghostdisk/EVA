# TODO

## Script

- `x.5` lexes as `x` followed by the number `.5` rather than a member access, since a `.` followed by a digit always starts a number.
- `a++.b` and `a++++` are accepted: a postfix operator can be followed by more postfix operators and member access.
- Support nested comments
- An attribute's expression swallows a following parenthesis or prefix operator: `@a (x)` parses as the call `a(x)` and `@a -x` as `a - x`. `@a (@b x)`, the example in ParseExpression's comment, parses as `a(@b x)` and then fails on the missing operand.
- A lone `;` (empty statement) is an error, so is a `;` after a statement ending with a block, e.g. `if a {};` (unlike `const`, where it's optional).
- Cap shader source size to a few MB, since shaders come from untrusted content.

## Core

- The atom table stores its strings in `std::string`s keyed by an `std::unordered_map`. Replace both: keep the strings in an arena owned by the table so they never move, and use our own hash map keyed by `StringView`. That removes the `std::string` built on every `GetAtom` lookup, and lets atom strings be returned as views without copying them into the caller's arena (e.g. in `SerializeNode`).