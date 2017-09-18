// Run: %dxc -T ps_6_0 -E main

// According to the HLSL reference, clip operates on scalar, vector, or matrix of floats.

// CHECK: [[v4f0:%\d+]] = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
// CHECK: [[v3f0:%\d+]] = OpConstantComposite %v3float %float_0 %float_0 %float_0

void main() {
  float    a;
  float4   b;
  float2x3 c;

// CHECK:                      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:          [[is_a_neg:%\d+]] = OpFOrdLessThan %bool [[a]] %float_0
// CHECK-NEXT:                              OpSelectionMerge %if_merge None
// CHECK-NEXT:                              OpBranchConditional [[is_a_neg]] %if_true %if_merge
// CHECK-NEXT:                   %if_true = OpLabel
// CHECK-NEXT:                              OpKill
// CHECK-NEXT:                  %if_merge = OpLabel
  clip(a);

// CHECK-NEXT:                 [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:      [[is_b_neg_vec:%\d+]] = OpFOrdLessThan %v4bool [[b]] [[v4f0]]
// CHECK-NEXT:          [[is_b_neg:%\d+]] = OpAny %bool [[is_b_neg_vec]]
// CHECK-NEXT:                              OpSelectionMerge %if_merge_0 None
// CHECK-NEXT:                              OpBranchConditional [[is_b_neg]] %if_true_0 %if_merge_0
// CHECK-NEXT:                 %if_true_0 = OpLabel
// CHECK-NEXT:                              OpKill
// CHECK-NEXT:                %if_merge_0 = OpLabel
  clip(b);

// CHECK:                      [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:            [[c_row0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT: [[is_c_row0_neg_vec:%\d+]] = OpFOrdLessThan %v3bool [[c_row0]] [[v3f0]]
// CHECK-NEXT:     [[is_c_row0_neg:%\d+]] = OpAny %bool [[is_c_row0_neg_vec]]
// CHECK-NEXT:            [[c_row1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT: [[is_c_row1_neg_vec:%\d+]] = OpFOrdLessThan %v3bool [[c_row1]] [[v3f0]]
// CHECK-NEXT:     [[is_c_row1_neg:%\d+]] = OpAny %bool [[is_c_row1_neg_vec]]
// CHECK-NEXT:      [[is_c_neg_vec:%\d+]] = OpCompositeConstruct %v2bool [[is_c_row0_neg]] [[is_c_row1_neg]]
// CHECK-NEXT:          [[is_c_neg:%\d+]] = OpAny %bool [[is_c_neg_vec]]
// CHECK-NEXT:                              OpSelectionMerge %if_merge_1 None
// CHECK-NEXT:                              OpBranchConditional [[is_c_neg]] %if_true_1 %if_merge_1
// CHECK-NEXT:                              %if_true_1 = OpLabel
// CHECK-NEXT:                              OpKill
  clip(c);
// CHECK-NEXT:                %if_merge_1 = OpLabel
// CHECK-NEXT:                              OpReturn
}
