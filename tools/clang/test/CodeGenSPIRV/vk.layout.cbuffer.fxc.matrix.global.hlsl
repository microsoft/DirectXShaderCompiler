// RUN: %dxc -T ps_6_0 -E main -fvk-use-dx-layout

// CHECK: OpDecorate [[arr_f2:%\w+]] ArrayStride 16
// CHECK: OpMemberDecorate {{%\w+}} 0 Offset 0
// CHECK: OpMemberDecorate {{%\w+}} 1 Offset 16
// CHECK: OpMemberDecorate {{%\w+}} 2 Offset 36

// CHECK: [[arr_f2]] = OpTypeArray %float %uint_2
// CHECK: %type__Globals = OpTypeStruct %float [[arr_f2]] %float
// CHECK: %_ptr_Uniform_type__Globals = OpTypePointer Uniform %type__Globals

// CHECK: [[Globals_clone:%\w+]] = OpTypeStruct %float %v2float %float
// CHECK: [[ptr_Globals_clone:%\w+]] = OpTypePointer Private [[Globals_clone]]

// CHECK: %_Globals = OpVariable %_ptr_Uniform_type__Globals Uniform
// CHECK:             OpVariable [[ptr_Globals_clone]] Private

float dummy0;                      // Offset:    0 Size:     4 [unused]
float1x2 foo;                      // Offset:   16 Size:    20 [unused]
float end;                         // Offset:   36 Size:     4

float4 main(float4 color : COLOR) : SV_TARGET
{
  color.x += end;
  return color;
}
