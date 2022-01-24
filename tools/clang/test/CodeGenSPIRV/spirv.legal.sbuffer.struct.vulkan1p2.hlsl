// RUN: %dxc -T cs_6_3 -fspv-target-env=vulkan1.2 -E main

// CHECK: %Struct = OpTypeStruct %v3float %_ptr_StorageBuffer_type_StructuredBuffer_uint
struct Struct
{
  float3 foo;
  StructuredBuffer<uint> buffer;
};

// CHECK: %Struct2 = OpTypeStruct %v3float %_ptr_StorageBuffer_type_StructuredBuffer_uint %_ptr_StorageBuffer_type_StructuredBuffer_uint
struct Struct2
{
  float3 foo;
  StructuredBuffer<uint> buffer1;
  StructuredBuffer<uint> buffer2;
};

// CHECK: %g_stuff_buffer = OpVariable %_ptr_StorageBuffer_type_StructuredBuffer_uint StorageBuffer
StructuredBuffer<uint> g_stuff_buffer;

// CHECK: %g_output = OpVariable %_ptr_StorageBuffer_type_RWStructuredBuffer_uint StorageBuffer
RWStructuredBuffer<uint> g_output;

[numthreads(1, 1, 1)]
void main()
{
// CHECK:          %s = OpVariable %_ptr_Function_Struct Function
  Struct s;

// CHECK: [[p0:%\d+]] = OpAccessChain %_ptr_Function__ptr_StorageBuffer_type_StructuredBuffer_uint %s %int_1
// CHECK:               OpStore [[p0]] %g_stuff_buffer
  s.buffer = g_stuff_buffer;

  Struct2 s2;
  s2.buffer1 = g_stuff_buffer;
  s2.buffer2 = g_stuff_buffer;

// CHECK: [[p1:%\d+]] = OpAccessChain %_ptr_Function__ptr_StorageBuffer_type_StructuredBuffer_uint %s %int_1
// CHECK: [[p2:%\d+]] = OpLoad %_ptr_StorageBuffer_type_StructuredBuffer_uint [[p1]]
// CHECK: [[p3:%\d+]] = OpAccessChain %_ptr_StorageBuffer_uint [[p2]] %int_0 %uint_0
// CHECK: [[p4:%\d+]] = OpLoad %uint [[p3]]
// CHECK: [[p5:%\d+]] = OpAccessChain %_ptr_StorageBuffer_uint %g_output %int_0 %uint_0
// CHECK:               OpStore [[p5]] [[p4]]
  g_output[0] = s.buffer[0];
}
