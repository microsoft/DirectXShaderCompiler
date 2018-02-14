// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability SubgroupBallotKHR
// CHECK: OpExtension "SPV_KHR_shader_ballot"

// CHECK: [[fnType:%\d+]] = OpTypeFunction %uint %_ptr_Function_uint

Texture2D gTex[5];
SamplerState gSamp;

float4 main(uint id : ID) : SV_Target {
// CHECK: OpFunctionCall %uint %fn_NonUniformResourceIndex
    return gTex[NonUniformResourceIndex(id)].Sample(gSamp, float2(1., 2.)) +
    // Make sure we are calling the same function
// CHECK: OpFunctionCall %uint %fn_NonUniformResourceIndex
           gTex[NonUniformResourceIndex(id)].Sample(gSamp, float2(3., 4.));
}

// CHECK:      %fn_NonUniformResourceIndex = OpFunction %uint None [[fnType]]
// CHECK-NEXT:          %index = OpFunctionParameter %_ptr_Function_uint
// CHECK-NEXT:     %bb_entry_0 = OpLabel
// CHECK-NEXT:                   OpBranch %while_check
// CHECK-NEXT:    %while_check = OpLabel
// CHECK-NEXT:                   OpLoopMerge %while_merge %while_continue None
// CHECK-NEXT:                   OpBranchConditional %true %while_body %while_merge
// CHECK-NEXT:     %while_body = OpLabel
// CHECK-NEXT:  [[index:%\d+]] = OpLoad %uint %index
// CHECK-NEXT:    [[ret:%\d+]] = OpSubgroupFirstInvocationKHR %uint [[index]]
// CHECK-NEXT:     [[eq:%\d+]] = OpIEqual %bool [[ret]] [[index]]
// CHECK-NEXT:                   OpSelectionMerge %if_merge None
// CHECK-NEXT:                   OpBranchConditional [[eq]] %if_true %if_merge
// CHECK-NEXT:        %if_true = OpLabel
// CHECK-NEXT:                   OpReturnValue [[index]]
// CHECK-NEXT:       %if_merge = OpLabel
// CHECK-NEXT:                   OpBranch %while_continue
// CHECK-NEXT: %while_continue = OpLabel
// CHECK-NEXT:                   OpBranch %while_check
// CHECK-NEXT:    %while_merge = OpLabel
// CHECK-NEXT:                   OpReturnValue %uint_0
// CHECK-NEXT:                   OpFunctionEnd
