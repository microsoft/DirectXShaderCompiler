// Run: %dxc -T vs_6_0 -E main -Oconfig=-O,--loop-unroll

// Note: The above recipe should unroll the loop, but should not
// reduce the whole shader to "return (3,3,3,3)"

float4 main() : SV_POSITION {
  int j = 0;
  int i = 0;

  [unroll(3)] for (i = 0; i < 3; ++i) {
    j++;
  }

  return float4(j, j, j, j);
}

// CHECK: OpSLessThan %bool %int_0 %int_3
// CHECK: OpBranch
// CHECK: OpLabel
// CHECK: OpIAdd %int %int_0 %int_1
// CHECK: OpIAdd %int %int_0 %int_1
// CHECK: OpBranch
// CHECK: OpLabel
// CHECK: OpSLessThan %bool {{%\d+}} %int_3
// CHECK: OpBranch
// CHECK: OpLabel
// CHECK: OpIAdd %int {{%\d+}} %int_1
// CHECK: OpIAdd %int {{%\d+}} %int_1
// CHECK: OpBranch
// CHECK: OpLabel
// CHECK: OpSLessThan %bool {{%\d+}} %int_3
// CHECK: OpBranch
// CHECK: OpLabel
// CHECK: OpIAdd %int {{%\d+}} %int_1
// CHECK: OpIAdd %int {{%\d+}} %int_1
// CHECK: OpBranch