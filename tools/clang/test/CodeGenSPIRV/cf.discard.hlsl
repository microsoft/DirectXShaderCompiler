// Run: %dxc -T ps_6_0 -E main

// According to the HLS spec, discard can only be called from a pixel shader.
// This translates to OpKill in SPIR-V. OpKill must be the last instruction in a block.

void main() {
  int a, b;
  bool cond = true;
  
  while(cond) {
// CHECK: %while_body = OpLabel
    if(a==b) {
// CHECK: %if_true = OpLabel
// CHECK-NEXT: OpKill
      {{discard;}}
      discard;  // No SPIR-V should be emitted for this statement.
      break;    // No SPIR-V should be emitted for this statement.
    } else {
// CHECK-NEXT: %if_false = OpLabel
      ++a;
// CHECK: OpKill
      discard;
      continue; // No SPIR-V should be emitted for this statement.
      --b;      // No SPIR-V should be emitted for this statement.
    }
// CHECK-NEXT: %if_merge = OpLabel

  }
// CHECK: %while_merge = OpLabel

}
