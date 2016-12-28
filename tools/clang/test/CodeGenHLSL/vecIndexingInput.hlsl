// RUN: %dxc -E main -T ps_5_1 %s | FileCheck %s

// 4 GEP for copy, last one for indexing.
// CHECK: getelementptr
// CHECK: getelementptr
// CHECK: getelementptr
// CHECK: getelementptr
// CHECK: getelementptr

struct Interpolants
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

uint i;
float4 main( Interpolants In ) : SV_TARGET
{
    return In.color[i];
}