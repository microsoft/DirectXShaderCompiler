// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.branch.hlsl

static int a, b, c;

// Note that preprocessor prepends a "#line 1 ..." line to the whole file and
// the compliation sees line numbers incremented by 1.

void main() {
// CHECK:       OpLine [[file]] 15 3
// CHECK-NEXT:  OpBranch %do_while_header
  do {
// CHECK:       OpLine [[file]] 15 6
// CHECK-NEXT:  OpLoopMerge %do_while_merge %do_while_continue None
// CHECK-NEXT:  OpBranch %do_while_body
    if (a < 27) {
      ++a;
// CHECK:       OpLine [[file]] 23 7
// CHECK-NEXT:  OpBranch %do_while_continue
      continue;
    }
    b += a;
// CHECK:       OpLine [[file]] 31 3
// CHECK-NEXT:  OpBranch %do_while_continue

// CHECK:       OpLine [[file]] 31 17
// CHECK-NEXT:  OpBranchConditional {{%\d+}} %do_while_header %do_while_merge
  } while (c < b);

// CHECK:       OpLine [[file]] 37 3
// CHECK-NEXT:  OpBranch %while_check
// CHECK:       OpLine [[file]] 37 14
// CHECK:       OpBranchConditional {{%\d+}} %while_body %while_merge
  while (a < c) {
// CHECK:       OpLine [[file]] 41 17
// CHECK-NEXT:  OpSelectionMerge %if_merge_1 None
// CHECK-NEXT:  OpBranchConditional {{%\d+}} %if_true_0 %if_false
    if (b < 34) {
      a = 99;
// CHECK:       OpLine [[file]] 46 25
// CHECK-NEXT:  OpSelectionMerge %if_merge_0 None
// CHECK-NEXT:  OpBranchConditional {{%\d+}} %if_true_1 %if_false_0
    } else if (a > 100) {
      a -= 20;
// CHECK:       OpLine [[file]] 50 7
// CHECK-NEXT:  OpBranch %while_merge
      break;
    } else {
      c = b;
// CHECK:       OpLine [[file]] 55 5
// CHECK-NEXT:  OpBranch %if_merge_0
    }
// CHECK:                        OpLine [[file]] 61 3
// CHECK-NEXT:                   OpBranch %while_continue
// CHECK-NEXT: %while_continue = OpLabel
// CHECK-NEXT:                   OpBranch %while_check
// CHECK-NEXT:    %while_merge = OpLabel
  }

// CHECK:       OpLine [[file]] 65 19
// CHECK-NEXT:  OpBranch %for_check
  for (int i = 0; i < 10 && float(a / b) < 2.7; ++i) {
// CHECK:       OpLine [[file]] 65 44
// CHECK-NEXT:  OpLoopMerge %for_merge %for_continue None
// CHECK-NEXT:  OpBranchConditional {{%\d+}} %for_body %for_merge
    c = a + 2 * b + c;
// CHECK:                      OpLine [[file]] 73 3
// CHECK-NEXT:                 OpBranch %for_continue
// CHECK-NEXT: %for_continue = OpLabel
  }
// CHECK:                      OpLine [[file]] 73 3
// CHECK-NEXT:                 OpBranch %for_check
// CHECK-NEXT:    %for_merge = OpLabel

  switch (a) {
  case 1:
    b = c;
// CHECK-NEXT: OpLine [[file]] 83 5
// CHECK-NEXT: OpBranch %switch_merge
    break;
  case 2:
    b = 2 * c;
// CHECK-NEXT: OpLine [[file]] 88 3
// CHECK-NEXT: OpBranch %switch_4
  case 4:
    b = b + 4;
    break;
  default:
    b = a;
// CHECK-NEXT: OpLine [[file]] 95 3
// CHECK-NEXT: OpBranch %switch_merge
  }
}
