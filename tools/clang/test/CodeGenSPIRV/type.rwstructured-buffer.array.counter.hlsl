// RUN: %dxc -T ps_6_6 -E main -fvk-allow-rwstructuredbuffer-arrays

struct PSInput
{
	uint idx : COLOR;
};

// CHECK: OpDecorate %g_rwbuffer DescriptorSet 2
// CHECK: OpDecorate %g_rwbuffer Binding 0
// CHECK: OpDecorate %counter_var_g_rwbuffer DescriptorSet 2
// CHECK: OpDecorate %counter_var_g_rwbuffer Binding 1

// CHECK: %g_rwbuffer = OpVariable %_ptr_Uniform__arr_type_RWStructuredBuffer_uint_uint_5 Uniform
// CHECK: %counter_var_g_rwbuffer = OpVariable %_ptr_Uniform__arr_type_ACSBuffer_counter_uint_5 Uniform
RWStructuredBuffer<uint> g_rwbuffer[5] : register(u0, space2);

float4 main(PSInput input) : SV_TARGET
{
// Correctly increment the counter.
// CHECK: [[ac1:%\w+]] = OpAccessChain %_ptr_Uniform_type_ACSBuffer_counter %counter_var_g_rwbuffer {{%\d+}}
// CHECK: [[ac2:%\w+]] = OpAccessChain %_ptr_Uniform_int [[ac1]] %uint_0
// CHECK: OpAtomicIAdd %int [[ac2]] %uint_1 %uint_0 %int_1
    g_rwbuffer[input.idx].IncrementCounter();

// Correctly access the buffer.
// CHECK: [[ac1:%\w+]] = OpAccessChain %_ptr_Uniform_type_RWStructuredBuffer_uint %g_rwbuffer {{%\d+}}
// CHECK: [[ac2:%\w+]] = OpAccessChain %_ptr_Uniform_uint [[ac1]] %int_0 %uint_0
// CHECK: OpLoad %uint [[ac2]]
    return g_rwbuffer[input.idx][0];
}
