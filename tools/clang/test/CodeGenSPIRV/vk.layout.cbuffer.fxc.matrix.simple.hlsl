// Run: %dxc -T ps_6_0 -E main -fvk-use-dx-layout

// CHECK: OpDecorate [[arr_f2:%\w+]] ArrayStride 16
// CHECK: OpMemberDecorate {{%\w+}} 0 Offset 0
// CHECK: OpMemberDecorate {{%\w+}} 1 Offset 16
// CHECK: OpMemberDecorate {{%\w+}} 2 Offset 36

// CHECK: [[arr_f2]] = OpTypeArray %float %uint_2
// CHECK: %type_buffer0 = OpTypeStruct %float [[arr_f2]] %float

cbuffer buffer0 {
  float dummy0;                      // Offset:    0 Size:     4 [unused]
  float1x2 foo;                      // Offset:   16 Size:    20 [unused]
  float end;                         // Offset:   36 Size:     4
};

float4 main(float4 color : COLOR) : SV_TARGET
{
  color.x += end;
  return color;
}
