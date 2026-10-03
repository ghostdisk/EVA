# Golden tests

Each `.eva` file here is a test case: `Test_Golden.cpp` compiles it and compares the output of each stage with a file
next to it, named after it, which holds the expected output. They run with the other Script tests, as
`Golden.<suite>.<name>`.

## Suites

- **`Shader/`**: compiled as a shader. Every case needs all of:
  - `.interface`: the shader interface, by `ShaderInterfaceToString`.
  - `.ir`: the generated IR, before indices are clamped, by `IRModuleToString`. It has to validate.
  - `.d3d11.hlsl`, `.msl`, `.spvasm`: what `CompileShader` gives for D3D11, Metal and Vulkan, every entry point after
    a comment naming it. SPIR-V is disassembled by our own disassembler (`SPIRVText.cpp`), which prints what
    SPIRV-Tools does; when the Vulkan SDK is installed the two are compared. Each entry point also has to pass the
    target's own tools when they're available: SPIRV-Tools' validator, fxc and Metal. For D3D11, fxc's reflection of
    the bind groups has to match ours. A target that fails, for what it doesn't support yet, has its errors there
    instead, each as `error: <message>` after a comment.
- **`Script/`**: compiled as a script. Every case needs a `.ir`.

`Golden.Files` fails for any file that isn't a case or an expected output of one, so a misnamed file can't go unchecked.

A comment at the top of a case says what it's for.

## Adding or changing a case

Write the `.eva`, then have the tests write the expected outputs and review them:

```
Build/Debug/ScriptTests --filter Golden --update
git diff EVA/Script/Test/Golden
```

`--update` creates missing files and rewrites ones that differ. Without it, a difference fails the test at the first
line that differs in the expected file, and a missing file fails it too.
