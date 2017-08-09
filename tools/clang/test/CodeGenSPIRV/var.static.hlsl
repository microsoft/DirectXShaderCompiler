// Run: %dxc -T vs_6_0 -E main

// CHECK: [[v3b0:%\d+]] = OpConstantNull %v3bool
// CHECK: [[v4f0:%\d+]] = OpConstantNull %v4float

// CHECK: %ga = OpVariable %_ptr_Private_int Private %int_6
static int ga = 6;
// CHECK: %gb = OpVariable %_ptr_Private_v3bool Private [[v3b0]]
static bool3 gb;
// The front end has no const evaluation support for HLSL specific types.
// So the following will ends up trying to create an OpStore into gc. We emit
// those initialization code at the beginning of the entry function.
// TODO: optimize this to emit initializer directly: need to fix either the
// general const evaluation in the front end or add const evaluation in our
// InitListHandler.
static float2x2 gc = {1, 2, 3, 4};

// CHECK: [[input:%\d+]] = OpVariable %_ptr_Input_int Input

// CHECK: %a = OpVariable %_ptr_Private_uint Private %uint_5
// CHECK: %b = OpVariable %_ptr_Private_v4float Private [[v4f0]]
// CHECK: %c = OpVariable %_ptr_Private_int Private

// CHECK: %init_done_c = OpVariable %_ptr_Private_bool Private %false

int main(int input: A) : B {
// CHECK-LABEL: %bb_entry = OpLabel

    // initialization of gc
// CHECK:      [[v2f12:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[v2f34:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[mat1234:%\d+]] = OpCompositeConstruct %mat2v2float [[v2f12]] [[v2f34]]
// CHECK-NEXT: OpStore %gc [[mat1234]]

    static uint a = 5;    // const init
    static float4 b;      // no init

// CHECK-NEXT: [[initdonec:%\d+]] = OpLoad %bool %init_done_c
// CHECK-NEXT: OpSelectionMerge %if_merge None
// CHECK-NEXT: OpBranchConditional [[initdonec]] %if_true %if_merge
// CHECK-NEXT: %if_true = OpLabel
// CHECK-NEXT: [[initc:%\d+]] = OpLoad %int [[input]]
// CHECK-NEXT: OpStore %c [[initc]]
// CHECK-NEXT: OpStore %init_done_c %true
// CHECK-NEXT: OpBranch %if_merge
    static int c = input; // var init
// CHECK-NEXT: %if_merge = OpLabel

    return input;
}
