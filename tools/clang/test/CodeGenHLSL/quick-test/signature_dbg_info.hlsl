// RUN: %dxc -E main -T ps_6_0 -Od -Zi  %s | FileCheck %s

// Make sure debug info for inp exist.
// CHECK: !DILocalVariable(tag: DW_TAG_arg_variable, name: "inp"


float4 main(float4 inp : COLOR) : SV_TARGET0 {
    float4 a = inp;
    return a;
}