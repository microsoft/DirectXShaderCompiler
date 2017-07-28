// Run: %dxc -T ps_6_0 -E main

// Stage IO variables
// CHECK-DAG: [[color:%\d+]] = OpVariable %_ptr_Input_float Input
// CHECK-DAG: [[target:%\d+]] = OpVariable %_ptr_Output_v4float Output

float4 main(float color: COLOR) : SV_TARGET {
// CHECK-LABEL: %bb_entry = OpLabel

// CHECK-NEXT: %val = OpVariable %_ptr_Function_float Function %float_0
    float val = 0.;
// CHECK-NEXT: %i = OpVariable %_ptr_Function_int Function %int_0
// CHECK-NEXT: %j = OpVariable %_ptr_Function_int Function %int_0
// CHECK-NEXT: %k = OpVariable %_ptr_Function_int Function %int_0

// CHECK-NEXT: [[color0:%\d+]] = OpLoad %float [[color]]
// CHECK-NEXT: [[lt0:%\d+]] = OpFOrdLessThan %bool [[color0]] %float_0_3
// CHECK-NEXT: OpSelectionMerge %if_merge None
// CHECK-NEXT: OpBranchConditional [[lt0]] %if_true %if_merge
    if (color < 0.3) {
// CHECK-LABEL: %if_true = OpLabel
// CHECK-NEXT: OpStore %val %float_1
        val = 1.;
// CHECK-NEXT: OpBranch %if_merge
    }
// CHECK-LABEL: %if_merge = OpLabel
// CHECK-NEXT: OpBranch %for_check

    // for-stmt following if-stmt
// CHECK-LABEL: %for_check = OpLabel
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %int %i
// CHECK-NEXT: [[lt1:%\d+]] = OpSLessThan %bool [[i0]] %int_10
// CHECK-NEXT: OpLoopMerge %for_merge %for_continue None
// CHECK-NEXT: OpBranchConditional [[lt1]] %for_body %for_merge
    for (int i = 0; i < 10; ++i) {
// CHECK-LABEL: %for_body = OpLabel
// CHECK-NEXT: [[color1:%\d+]] = OpLoad %float [[color]]
// CHECK-NEXT: [[lt2:%\d+]] = OpFOrdLessThan %bool [[color1]] %float_0_5
// CHECK-NEXT: OpSelectionMerge %if_merge_0 None
// CHECK-NEXT: OpBranchConditional [[lt2]] %if_true_0 %if_merge_0
        if (color < 0.5) { // if-stmt nested in for-stmt
// CHECK-LABEL: %if_true_0 = OpLabel
// CHECK-NEXT: [[val0:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[add1:%\d+]] = OpFAdd %float [[val0]] %float_1
// CHECK-NEXT: OpStore %val [[add1]]
            val = val + 1.;
// CHECK-NEXT: OpBranch %for_check_0

// CHECK-LABEL: %for_check_0 = OpLabel
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %int %j
// CHECK-NEXT: [[lt3:%\d+]] = OpSLessThan %bool [[j0]] %int_15
// CHECK-NEXT: OpLoopMerge %for_merge_0 %for_continue_0 None
// CHECK-NEXT: OpBranchConditional [[lt3]] %for_body_0 %for_merge_0
            for (int j = 0; j < 15; ++j) { // for-stmt deeply nested in if-then
// CHECK-LABEL: %for_body_0 = OpLabel
// CHECK-NEXT: [[val1:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[mul2:%\d+]] = OpFMul %float [[val1]] %float_2
// CHECK-NEXT: OpStore %val [[mul2]]
                val = val * 2.;
// CHECK-NEXT: OpBranch %for_continue_0

// CHECK-LABEL: %for_continue_0 = OpLabel
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %int %j
// CHECK-NEXT: [[incj:%\d+]] = OpIAdd %int [[j1]] %int_1
// CHECK-NEXT: OpStore %j [[incj]]
// CHECK-NEXT: OpBranch %for_check_0
            } // end for (int j
// CHECK-LABEL: %for_merge_0 = OpLabel
// CHECK-NEXT: [[val2:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[add3:%\d+]] = OpFAdd %float [[val2]] %float_3
// CHECK-NEXT: OpStore %val [[add3]]

            val = val + 3.;
// CHECK-NEXT: OpBranch %if_merge_0
        }
// CHECK-LABEL: %if_merge_0 = OpLabel

// CHECK-NEXT: [[color2:%\d+]] = OpLoad %float [[color]]
// CHECK-NEXT: [[lt4:%\d+]] = OpFOrdLessThan %bool [[color2]] %float_0_8
// CHECK-NEXT: OpSelectionMerge %if_merge_1 None
// CHECK-NEXT: OpBranchConditional [[lt4]] %if_true_1 %if_false
        if (color < 0.8) { // if-stmt following if-stmt
// CHECK-LABEL: %if_true_1 = OpLabel
// CHECK-NEXT: [[val3:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[mul4:%\d+]] = OpFMul %float [[val3]] %float_4
// CHECK-NEXT: OpStore %val [[mul4]]
            val = val * 4.;
// CHECK-NEXT: OpBranch %if_merge_1
        } else {
// CHECK-LABEL: %if_false = OpLabel
// CHECK-NEXT: OpBranch %for_check_1

// CHECK-LABEL: %for_check_1 = OpLabel
// CHECK-NEXT: [[k0:%\d+]] = OpLoad %int %k
// CHECK-NEXT: [[lt5:%\d+]] = OpSLessThan %bool [[k0]] %int_20
// CHECK-NEXT: OpLoopMerge %for_merge_1 %for_continue_1 None
// CHECK-NEXT: OpBranchConditional [[lt5]] %for_body_1 %for_merge_1
            for (int k = 0; k < 20; ++k) { // for-stmt deeply nested in if-else
// CHECK-LABEL: %for_body_1 = OpLabel
// CHECK-NEXT: [[val4:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[sub5:%\d+]] = OpFSub %float [[val4]] %float_5
// CHECK-NEXT: OpStore %val [[sub5]]
                val = val - 5.;

// CHECK-NEXT: [[val5:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[lt6:%\d+]] = OpFOrdLessThan %bool [[val5]] %float_0
// CHECK-NEXT: OpSelectionMerge %if_merge_2 None
// CHECK-NEXT: OpBranchConditional [[lt6]] %if_true_2 %if_merge_2
                if (val < 0.) { // deeply nested if-stmt
// CHECK-LABEL: %if_true_2 = OpLabel
// CHECK-NEXT: [[val6:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[add100:%\d+]] = OpFAdd %float [[val6]] %float_100
// CHECK-NEXT: OpStore %val [[add100]]
                    val = val + 100.;
// CHECK-NEXT: OpBranch %if_merge_2
                }
// CHECK-LABEL: %if_merge_2 = OpLabel
// CHECK-NEXT: OpBranch %for_continue_1

// CHECK-LABEL: %for_continue_1 = OpLabel
// CHECK-NEXT: [[k1:%\d+]] = OpLoad %int %k
// CHECK-NEXT: [[inck:%\d+]] = OpIAdd %int [[k1]] %int_1
// CHECK-NEXT: OpStore %k [[inck]]
// CHECK-NEXT: OpBranch %for_check_1
            } // end for (int k
// CHECK-LABEL: %for_merge_1 = OpLabel
// CHECK-NEXT: OpBranch %if_merge_1
        } // end elsek
// CHECK-LABEL: %if_merge_1 = OpLabel
// CHECK-NEXT: OpBranch %for_continue

// CHECK-LABEL: %for_continue = OpLabel
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %int %i
// CHECK-NEXT: [[inci:%\d+]] = OpIAdd %int [[i1]] %int_1
// CHECK-NEXT: OpStore %i [[inci]]
// CHECK-NEXT: OpBranch %for_check
    } // end for (int i
// CHECK-LABEL: %for_merge = OpLabel

    // if-stmt following for-stmt
// CHECK-NEXT: [[color3:%\d+]] = OpLoad %float [[color]]
// CHECK-NEXT: [[lt7:%\d+]] = OpFOrdLessThan %bool [[color3]] %float_0_9
// CHECK-NEXT: OpSelectionMerge %if_merge_3 None
// CHECK-NEXT: OpBranchConditional [[lt7]] %if_true_3 %if_merge_3
    if (color < 0.9) {
// CHECK-LABEL: %if_true_3 = OpLabel
// CHECK-NEXT: [[val7:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[add6:%\d+]] = OpFAdd %float [[val7]] %float_6
// CHECK-NEXT: OpStore %val [[add6]]
        val = val + 6.;
// CHECK-NEXT: OpBranch %if_merge_3
    }
// CHECK-LABEL: %if_merge_3 = OpLabel

// CHECK-NEXT: [[comp0:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[comp1:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[comp2:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[comp3:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[ret:%\d+]] = OpCompositeConstruct %v4float [[comp0]] [[comp1]] [[comp2]] [[comp3]]
// CHECK-NEXT: OpStore [[target]] [[ret]]
// CHECK-NEXT: OpReturn
    return float4(val, val, val, val);
// CHECK-NEXT: OpFunctionEnd
}
