// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.variables.hlsl

struct S {
  float4 f;
};

// CHECK:                        OpLine [[file]] 12 32
// CHECK-NEXT: %consume_v2bool = OpVariable %_ptr_Uniform_type_ConsumeStructuredBuffer_v2bool Uniform
ConsumeStructuredBuffer<bool2> consume_v2bool;

// CHECK:                        OpLine [[file]] 16 32
// CHECK-NEXT: %append_v2float = OpVariable %_ptr_Uniform_type_AppendStructuredBuffer_v2float Uniform
AppendStructuredBuffer<float2> append_v2float;

// CHECK:                       OpLine [[file]] 20 19
// CHECK-NEXT: %byte_addr_buf = OpVariable %_ptr_Uniform_type_ByteAddressBuffer Uniform
ByteAddressBuffer byte_addr_buf;

// CHECK:                    OpLine [[file]] 24 19
// CHECK-NEXT: %rw_texture = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
RWTexture2D<int3> rw_texture;

// TODO(jaebaek): Check DeclResultIdMapper::create.*Var()
//                we must emit "OpLine .." here.
TextureBuffer<S> texture_buf;

// CHECK:             OpLine [[file]] 32 14
// CHECK-NEXT: %sam = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
SamplerState sam;

// CHECK:                     OpLine [[file]] 36 19
// CHECK-NEXT: %float_array = OpVariable %_ptr_Workgroup__arr_float_uint_10 Workgroup
groupshared float float_array[10];

// CHECK:                     OpLine [[file]] 40 12
// CHECK-NEXT:   %int_array = OpVariable %_ptr_Private__arr_int_uint_10 Private
static int int_array[10];

// "static" variables in functions must be defined first.
// CHECK:                     OpLine [[file]] 61 16
// CHECK-NEXT:           %c = OpVariable %_ptr_Private_v3bool Private
// CHECK-NEXT: %init_done_c = OpVariable %_ptr_Private_bool Private %false

bool3 test_function_variables() {
// CHECK:                     OpLine [[file]] 50 9
// CHECK-NEXT:         %a_0 = OpVariable %_ptr_Function_v3bool Function
  bool3 a;

// CHECK-NEXT:                OpLine [[file]] 54 22
// CHECK-NEXT:         %b_0 = OpVariable %_ptr_Function_mat2v3float Function
  row_major float2x3 b;

// CHECK:                      OpLine [[file]] 61 20
// CHECK-NEXT: [[init:%\d+]] = OpLoad %bool %init_done_c
// CHECK-NEXT:                 OpSelectionMerge %if_init_done None
// CHECK-NEXT:                 OpBranchConditional [[init]] %if_init_done %if_init_todo
// CHECK-NEXT: %if_init_todo = OpLabel
  static bool3 c = bool3(true, consume_v2bool.Consume());

// CHECK:      %if_init_done = OpLabel

  c = a && c;
  return c;
}

// CHECK:                     OpLine [[file]] 75 1
// CHECK-NEXT: %test_function_param = OpFunction %v2float None
// CHECK-NEXT:                OpLine [[file]] 75 35
// CHECK-NEXT:         %a_1 = OpFunctionParameter %_ptr_Function_v2float
// CHECK-NEXT:                OpLine [[file]] 75 50
// CHECK-NEXT:         %b_1 = OpFunctionParameter %_ptr_Function_v3bool
float2 test_function_param(float2 a, inout bool3 b,
// CHECK-NEXT:                OpLine [[file]] 80 38
// CHECK-NEXT:         %c_1 = OpFunctionParameter %_ptr_Function_int
// CHECK-NEXT:                OpLine [[file]] 80 52
// CHECK-NEXT:         %d_0 = OpFunctionParameter %_ptr_Function_v3float
                           const int c, out float3 d,
// CHECK-NEXT:                OpLine [[file]] 83 33
// CHECK-NEXT:           %e = OpFunctionParameter %_ptr_Function_bool
                           bool e) {
// CHECK:                     OpLine [[file]] 86 9
// CHECK-NEXT:           %f = OpVariable %_ptr_Function__arr_float_uint_2 Function
  float f[2];
  return a + d.xy + float2(f);
}

void main() {
  bool a;
  bool3 b = test_function_variables();
  float3 c;
  float2 d = test_function_param(float2(1, 0), b, 13, c, a);
}
