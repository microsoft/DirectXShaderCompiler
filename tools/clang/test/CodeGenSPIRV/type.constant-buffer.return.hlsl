// RUN: %dxc -T vs_6_0 -E main

// CHECK:      OpName %type_ConstantBuffer_S "type.ConstantBuffer.S"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_S 0 "someFloat"
// CHECK-NEXT: OpMemberName %type_ConstantBuffer_S 1 "another"

// CHECK:      OpDecorate %type_ConstantBuffer_S Block

// CHECK: %type_ConstantBuffer_S = OpTypeStruct %float %float

struct S
{
    float someFloat;
    float another;
};

// CHECK: %_runtimearr_type_ConstantBuffer_S = OpTypeRuntimeArray %type_ConstantBuffer_S
// CHECK: %_ptr_Uniform__runtimearr_type_ConstantBuffer_S = OpTypePointer Uniform %_runtimearr_type_ConstantBuffer_S

// CHECK: %S = OpTypeStruct %float %float
// CHECK: %_ptr_Function_S = OpTypePointer Function %S

// CHECK: %buffers = OpVariable %_ptr_Uniform__runtimearr_type_ConstantBuffer_S Uniform
ConstantBuffer<S> buffers[];

ConstantBuffer<S> getBuf(uint indx)
{
// CHECK-DAG: %temp_var_ret = OpVariable %_ptr_Function_S Function
// CHECK-DAG: [[bufPtr:%\d+]] = OpAccessChain %_ptr_Uniform_S %buffers [[indx:%\d+]]
// CHECK-DAG: [[bufVal:%\d+]] = OpLoad %S [[bufPtr]]
// CHECK-DAG: OpStore %temp_var_ret [[bufVal]]
    return buffers[indx];
};

void main()
{
// CHECK-DAG: [[val:%\d+]] = OpFunctionCall %S %getBuf %param_var_indx
  getBuf(1);
}
