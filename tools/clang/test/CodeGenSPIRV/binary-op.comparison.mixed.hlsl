// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    bool3 r;
    float3 a;
    float s;

// CHECK:      [[a0:%\d+]] = OpLoad %v3float %a
// CHECK-NEXT: [[s0:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v3float [[s0]] [[s0]] [[s0]]
// CHECK-NEXT: [[lt0:%\d+]] = OpFOrdLessThan %v3bool [[a0]] [[cc0]]
// CHECK-NEXT: OpStore %r [[lt0]]
    r = a < s;
// CHECK-NEXT: [[s1:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v3float [[s1]] [[s1]] [[s1]]
// CHECK-NEXT: [[a1:%\d+]] = OpLoad %v3float %a
// CHECK-NEXT: [[lt1:%\d+]] = OpFOrdLessThan %v3bool [[cc1]] [[a1]]
// CHECK-NEXT: OpStore %r [[lt1]]
    r = s < a;
}
