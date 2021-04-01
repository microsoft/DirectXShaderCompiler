// Run: %dxc -T ps_6_0 -E main -fvk-use-dx-layout

// CHECK: OpDecorate [[arr_f2:%\w+]] ArrayStride 16
// CHECK: OpMemberDecorate %layout 0 Offset 0
// CHECK: OpMemberDecorate %layout 1 Offset 16
// CHECK: OpMemberDecorate %layout 2 Offset 36
// CHECK: OpMemberDecorate %type_buffer0 0 Offset 0
// CHECK: OpMemberDecorate %type_buffer0 1 Offset 16
// CHECK: OpMemberDecorate %type_buffer0 2 Offset 56

// CHECK: [[arr_f2]] = OpTypeArray %float %uint_2
// CHECK: %layout = OpTypeStruct %float [[arr_f2]] %float

cbuffer buffer0 {
  float dummy0;                      // Offset:    0 Size:     4 [unused]
  struct layout
  {
      float1x1 dummy0;               // Offset:   16
      float1x2 foo;                  // Offset:   32
      float end;                     // Offset:   52

  } bar;                             // Offset:   16 Size:    40 [unused]
  float end;                         // Offset:   56 Size:     4
};

float4 main(float4 color : COLOR) : SV_TARGET
{
  color.x += end;

// CHECK: [[ptr_layout:%\w+]] = OpTypePointer Uniform %layout
// CHECK: [[buffer0_1:%\w+]] = OpAccessChain [[ptr_layout]] %buffer0 %int_1
// CHECK: [[ptr_foo:%\w+]] = OpAccessChain {{%\w+}} [[buffer0_1]] %int_1
// CHECK: [[ptr_foo_1:%\w+]] = OpAccessChain {{%\w+}} [[ptr_foo]] %int_1
// CHECK: OpLoad %float [[ptr_foo_1]]
  color.x += bar.foo._12;

  return color;
}
