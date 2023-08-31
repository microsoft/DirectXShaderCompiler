// RUN: %dxc -T ps_6_0 -E main

// CHECK-NOT: OpDecorate {{%\w+}} BuiltIn HelperInvocation

float4 main() : SV_Target {
    float ret = 1.0;

    if (IsHelperLane()) ret = 2.0;

    return ret;
}
// CHECK: [[HelperInvocation:%\d+]] = OpIsHelperInvocationEXT %bool
// CHECK: OpBranchConditional [[HelperInvocation]]
