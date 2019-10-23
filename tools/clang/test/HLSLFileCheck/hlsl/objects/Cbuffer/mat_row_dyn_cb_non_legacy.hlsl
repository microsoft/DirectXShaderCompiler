// RUN: %dxc -E main -T cs_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer C
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct dx.alignment.legacy.C
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       row_major int3x4 m;                           ; Offset:    0
// CHECK: ;
// CHECK: ;   } C;                                              ; Offset:    0 Size:    48
// CHECK: ;
// CHECK: ; }
// CHECK: ;
// CHECK: ; Resource bind info for output
// CHECK: ; {
// CHECK: ;
// CHECK: ;   int $Element;                                     ; Offset:    0 Size:     4
// CHECK: ;
// CHECK: ; }

// One row should be 4 elements, loaded into array for component indexing
// CHECK: alloca [4 x i32]

// CHECK: @dx.op.cbufferLoad.i32
// CHECK: @dx.op.cbufferLoad.i32
// CHECK: @dx.op.cbufferLoad.i32
// CHECK: @dx.op.cbufferLoad.i32

cbuffer C
{
    row_major int3x4 m;
};

RWStructuredBuffer<int> output;

[shader("compute")]
[numthreads(12,1,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint i = tid.x;

    output[i] = m[i / 4][i % 4];
}
