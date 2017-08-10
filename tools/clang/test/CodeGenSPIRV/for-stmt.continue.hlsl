// Run: %dxc -T ps_6_0 -E main

void main() {
  int val = 0;
  
// CHECK:      OpBranch %for_check
// CHECK-NEXT: %for_check = OpLabel
// CHECK:      OpLoopMerge %for_merge %for_continue None
// CHECK-NEXT: OpBranchConditional {{%\d+}} %for_body %for_merge
  for (int i = 0; i < 10; ++i) {
// CHECK-NEXT: %for_body = OpLabel
// CHECK:      OpSelectionMerge %if_merge None
// CHECK-NEXT: OpBranchConditional {{%\d+}} %if_true %if_merge
    if (i < 5) {
// CHECK-NEXT: %if_true = OpLabel
// CHECK-NEXT: OpBranch %for_continue
      continue;
    }
// CHECK-NEXT: %if_merge = OpLabel
    val = i;
// CHECK:      OpBranch %for_continue
    {continue;}
    val++;       // No SPIR-V should be emitted for this statement.
    continue;    // No SPIR-V should be emitted for this statement.
    while(true); // No SPIR-V should be emitted for this statement.

// CHECK-NEXT: %for_continue = OpLabel
// CHECK:      OpBranch %for_check
  }
// CHECK-NEXT: %for_merge = OpLabel

// CHECK-NEXT: OpBranch %for_check_0



  //////////////////////////////////////////////////////////////////////////////////////
  // Nested for loops with continue statements                                        //
  // Each continue statement should branch to the corresponding loop's continue block //
  //////////////////////////////////////////////////////////////////////////////////////

// CHECK-NEXT: %for_check_0 = OpLabel
// CHECK:      OpLoopMerge %for_merge_0 %for_continue_0 None
// CHECK-NEXT: OpBranchConditional {{%\d+}} %for_body_0 %for_merge_0
  for (int j = 0; j < 10; ++j) {
// CHECK-NEXT: %for_body_0 = OpLabel
    val = j+5;
// CHECK:      OpBranch %for_check_1

// CHECK-NEXT: %for_check_1 = OpLabel
// CHECK:      OpLoopMerge %for_merge_1 %for_continue_1 None
// CHECK-NEXT: OpBranchConditional {{%\d+}} %for_body_1 %for_merge_1
    for ( ; val < 20; ++val) {
// CHECK-NEXT: %for_body_1 = OpLabel
      int k = val + j;
// CHECK:      OpBranch %for_continue_1
      continue;
      k++;      // No SPIR-V should be emitted for this statement.

// CHECK-NEXT: %for_continue_1 = OpLabel
// CHECK:      OpBranch %for_check_1
    }
// CHECK-NEXT: %for_merge_1 = OpLabel
    val--;
// CHECK:      OpBranch %for_continue_0
    continue;
    continue;     // No SPIR-V should be emitted for this statement.
    val = val*10; // No SPIR-V should be emitted for this statement.

// CHECK-NEXT: %for_continue_0 = OpLabel
// CHECK:      OpBranch %for_check_0
  }
// CHECK-NEXT: %for_merge_0 = OpLabel
}
