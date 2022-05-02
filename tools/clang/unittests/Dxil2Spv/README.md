# `dxil2spv` Tests

## How to Add a Test

These instructions assume the new test will be generated from HLSL source,
rather than hand-writing DXIL directly. Whenever this is the case, the HLSL
source should be preserved to aid in regenerating tests in future as needed
(i.e. if changes are made to `dxc` that warrant recompiling, or a new test needs
to be created as a modified version of an existing test).

1.  Create a new HLSL source file in `tools/clang/test/Dxil2Spv/`.

    For example, `my-new-test.hlsl`:

    ```
    struct VSOutput {
        float4 Position : SV_POSITION;
        float3 Color    : COLOR;
    };

    float4 main(VSOutput input) : SV_TARGET
    {
        return float4(input.Color, 1);
    };
    ```

2.  Compile HLSL source file to DXIL.

    Use the appropriate `dxc` command to compile the HLSL source and output the
    disassembled DXIL to a file with the same name and a `.ll` extension using
    the option `-Fc`.

    For example: `dxc -T ps_6_0 my-new-test.hlsl -Fc my-new-test.ll`

3.  Add a comment the to the top of your HLSL source file with the compile
    command used.

    `my-new-test.hlsl`:

    ```
    // dxc -T ps_6_0 my-new-test.hlsl -Fc my-new-test.ll
    struct VSOutput {
    [...]
    ```

4.  Add a `; RUN: %dxil2spv` command to the top of your DXIL test file.

    `my-new-test.ll`:

    ```
    ; RUN: %dxil2spv
    ;
    ; Input signature:
    [...]
    ```

5.  Add `CHECK` lines to test the generated SPIR-V.

    As needed for your test, add `CHECK` lines throughout the DXIL test file.
    These tests are run with `effcee`, which supports pattern matching of
    strings inspired by LLVM's FileCheck command. See the `effcee` documentation
    for more information about the `CHECK` types supported and pattern matching
    syntax.

6.  Add the test to `tools/clang/unittests/Dxil2Spv/LitTest.cpp`.

    For example:

    ```
    TEST_F(FileTest, MyNewTest) {
      runFileTest("my-new-test.ll");
    }
    ```
