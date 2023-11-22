// RUN: %dxc -T ps_6_6 -E main -HV 2021

struct S1 {
    uint f1 : 1;
    uint f2 : 1;
};

Buffer<S1> input_1;

void main() {
// CHECK: [[img:%\d+]] = OpLoad %type_buffer_image %input_1
// CHECK: [[tmp:%\d+]] = OpImageFetch %v4uint [[img]] %uint_0 None
// CHECK: [[tmp:%\d+]] = OpVectorShuffle %v2uint [[tmp]] [[tmp]] 0 1
// CHECK: [[tmp_f1:%\d+]] = OpCompositeExtract %uint [[tmp]] 0
// CHECK: [[tmp_s1:%\d+]] = OpCompositeConstruct %S1 [[tmp_f1]]
// CHECK: OpStore [[tmp_var_S1:%\w+]] [[tmp_s1]]
// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint [[tmp_var_S1]] %int_0
// CHECK: [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[extract:%\d+]] = OpBitFieldUExtract %uint [[load]] %uint_1 %uint_1
// CHECK: OpStore %tmp [[extract]]
  uint tmp = input_1[0].f2;
}
