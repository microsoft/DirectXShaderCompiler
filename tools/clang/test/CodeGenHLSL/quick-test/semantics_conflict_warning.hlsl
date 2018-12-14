// RUN: %dxc -T ps_6_0 -E main -WX %s | FileCheck %s

// CHECK: semantic 'COLOR0' on struct field will be ignored

struct Input
{
    float a;
    float b : COLOR0;
};

float main(Input input : TEXCOORD0) : SV_Target
{
    return 1;
}