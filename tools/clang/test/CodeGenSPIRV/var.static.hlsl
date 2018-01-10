// Run: %dxc -T vs_6_0 -E main

// CHECK: [[v3b0:%\d+]] = OpConstantNull %v3bool
// CHECK: [[v4f0:%\d+]] = OpConstantNull %v4float

// CHECK: %ga = OpVariable %_ptr_Private_int Private
static int ga = 6;
// CHECK: %gb = OpVariable %_ptr_Private_v3bool Private
static bool3 gb;
// The front end has no const evaluation support for HLSL specific types.
// So the following will ends up trying to create an OpStore into gc. We emit
// those initialization code at the beginning of the entry function.
// TODO: optimize this to emit initializer directly: need to fix either the
// general const evaluation in the front end or add const evaluation in our
// InitListHandler.
static float2x2 gc = {1, 2, 3, 4};

// CHECK: %a = OpVariable %_ptr_Private_uint Private
// CHECK: %init_done_a = OpVariable %_ptr_Private_bool Private %false
// CHECK: %b = OpVariable %_ptr_Private_v4float Private
// CHECK: %init_done_b = OpVariable %_ptr_Private_bool Private %false
// CHECK: %c = OpVariable %_ptr_Private_int Private
// CHECK: %init_done_c = OpVariable %_ptr_Private_bool Private %false

    // initialization of ga, gb, and gc appears at the beginning of the entry function wrapper
// CHECK-LABEL: OpLabel
// CHECK:      OpStore %ga %int_6
// CHECK-NEXT: OpStore %gb [[v3b0]]
// CHECK-NEXT: [[v2f12:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[v2f34:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[mat1234:%\d+]] = OpCompositeConstruct %mat2v2float [[v2f12]] [[v2f34]]
// CHECK-NEXT: OpStore %gc [[mat1234]]
// CHECK:      OpFunctionCall %int %src_main
// CHECK-LABEL: OpFunctionEnd

int main(int input: A) : B {
// CHECK-LABEL: %bb_entry = OpLabel

// CHECK-NEXT: [[initdonea:%\d+]] = OpLoad %bool %init_done_a
// CHECK-NEXT: OpSelectionMerge %if_init_done None
// CHECK-NEXT: OpBranchConditional [[initdonea]] %if_init_done %if_init_todo
// CHECK-NEXT: %if_init_todo = OpLabel
// CHECK-NEXT: OpStore %a %uint_5
// CHECK-NEXT: OpStore %init_done_a %true
// CHECK-NEXT: OpBranch %if_init_done
    static uint a = 5;    // const init
// CHECK-NEXT: %if_init_done = OpLabel

// CHECK-NEXT: [[initdoneb:%\d+]] = OpLoad %bool %init_done_b
// CHECK-NEXT: OpSelectionMerge %if_init_done_0 None
// CHECK-NEXT: OpBranchConditional [[initdoneb]] %if_init_done_0 %if_init_todo_0
// CHECK-NEXT: %if_init_todo_0 = OpLabel
// CHECK-NEXT: OpStore %b [[v4f0]]
// CHECK-NEXT: OpStore %init_done_b %true
// CHECK-NEXT: OpBranch %if_init_done_0
    static float4 b;      // no init
// CHECK-NEXT: %if_init_done_0 = OpLabel

// CHECK-NEXT: [[initdonec:%\d+]] = OpLoad %bool %init_done_c
// CHECK-NEXT: OpSelectionMerge %if_init_done_1 None
// CHECK-NEXT: OpBranchConditional [[initdonec]] %if_init_done_1 %if_init_todo_1
// CHECK-NEXT: %if_init_todo_1 = OpLabel
// CHECK-NEXT: [[initc:%\d+]] = OpLoad %int %input
// CHECK-NEXT: OpStore %c [[initc]]
// CHECK-NEXT: OpStore %init_done_c %true
// CHECK-NEXT: OpBranch %if_init_done_1
    static int c = input; // var init
// CHECK-NEXT: %if_init_done_1 = OpLabel

    return input;
}
