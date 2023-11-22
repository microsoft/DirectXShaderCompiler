// RUN: %dxc -T vs_6_6 -E main -HV 2021

// CHECK: [[type_S:%\w+]] = OpTypeStruct %uint %uint %uint
// CHECK: [[rarr_S:%\w+]] = OpTypeRuntimeArray [[type_S]]
// CHECK: [[buffer:%\w+]] = OpTypeStruct [[rarr_S]]
// CHECK: [[ptr_buffer:%\w+]] = OpTypePointer Uniform [[buffer]]
struct S {
    uint f1;
    uint f2 : 1;
    uint f3 : 1;
    uint f4;
};

// CHECK: [[var_buffer:%\w+]] = OpVariable [[ptr_buffer]] Uniform
StructuredBuffer<S> buffer;

void main(uint id : A) {
  // CHECK: [[id:%\d+]] = OpLoad %uint %id
  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[var_buffer]] %int_0 [[id]] %int_1
  // CHECK: [[value:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[value:%\d+]] = OpBitFieldUExtract %uint [[value]] %uint_1 %uint_1
  // CHECK: OpStore %tmp [[value]]
  uint tmp = buffer[id].f3;
}
