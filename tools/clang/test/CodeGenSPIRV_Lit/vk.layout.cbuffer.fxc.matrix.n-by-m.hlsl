// RUN: %dxc -T ps_6_0 -E main -fvk-use-dx-layout


// CHECK: OpMemberDecorate {{%\w+}} 0 Offset 0
// CHECK: OpMemberDecorate {{%\w+}} 1 Offset 16
// CHECK: OpMemberDecorate {{%\w+}} 1 MatrixStride 16
// CHECK: OpMemberDecorate {{%\w+}} 1 RowMajor
// CHECK: OpMemberDecorate {{%\w+}} 2 Offset 56

cbuffer buffer0 {
  float dummy0;                      // Offset:    0 Size:     4 [unused]
  float2x3 foo;                      // Offset:   16 Size:    40 [unused]
  float end;                         // Offset:   56 Size:     4
};

float4 main(float4 color : COLOR) : SV_TARGET
{
  color.x += end;
  return color;
}
