// RUN: %dxc -HV 2021 -T cs_6_7 -E main

// CHECK: [[static_var:%\w+]] = OpVariable %_ptr_Private_int Private
// CHECK: [[init_var:%\w+]] = OpVariable %_ptr_Private_bool Private %false
// CHECK: %test = OpFunction %int None
// CHECK: [[init:%\w+]] = OpLoad %bool [[init_var]]
// CHECK: OpBranchConditional [[init]] [[init_done_bb:%\w+]] [[do_init_bb:%\w+]]
// CHECK: [[do_init_bb]] = OpLabel
// CHECK: OpStore [[static_var]] %int_20
// CHECK: OpStore [[init_var]] %true
// CHECK: OpBranch [[init_done_bb]]
// CHECK: [[init_done_bb]] = OpLabel
// CHECK: OpLoad %int [[static_var]]


template <typename R> R test(R x) {
    static R v = 20;
    return v * x;
}

[numthreads(32, 32, 1)] void main(uint2 threadId: SV_DispatchThreadID) {
    float x = test(10);
}

