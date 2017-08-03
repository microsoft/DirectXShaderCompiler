// Run: %dxc -T vs_6_0 -E main

// CHECK: [[v2f10_3:%\d+]] = OpConstantComposite %v2float %float_10_3 %float_10_3
// CHECK: [[v3f10_4:%\d+]] = OpConstantComposite %v3float %float_10_4 %float_10_4 %float_10_4
// CHECK: [[v2f10_5:%\d+]] = OpConstantComposite %v2float %float_10_5 %float_10_5
// CHECK: [[m3v2f10_5:%\d+]] = OpConstantComposite %mat3v2float [[v2f10_5]] [[v2f10_5]] [[v2f10_5]]

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // TODO: Optimally the following literals can be attached to variable
    // definitions instead of OpStore. Constant evaluation in the front
    // end doesn't really support it for now.

// CHECK:      OpStore %a %float_10_2
    float1x1 a = 10.2;
// CHECK-NEXT: OpStore %b [[v2f10_3]]
    float1x2 b = 10.3;
// CHECK-NEXT: OpStore %c [[v3f10_4]]
    float3x1 c = 10.4;
// CHECK-NEXT: OpStore %d [[m3v2f10_5]]
    float3x2 d = 10.5;

    float val;
// CHECK-NEXT: [[val0:%\d+]] = OpLoad %float %val
// CHECK-NEXT: OpStore %h [[val0]]
    float1x1 h = val;
// CHECK-NEXT: [[val1:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v3float [[val1]] [[val1]] [[val1]]
// CHECK-NEXT: OpStore %i [[cc0]]
    float1x3 i = val;
    float2x1 j;
    float2x3 k;

// CHECK-NEXT: [[val2:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v2float [[val2]] [[val2]]
// CHECK-NEXT: OpStore %j [[cc1]]
    j = val;
// CHECK-NEXT: [[val3:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[cc2:%\d+]] = OpCompositeConstruct %v3float [[val3]] [[val3]] [[val3]]
// CHECK-NEXT: [[cc3:%\d+]] = OpCompositeConstruct %mat2v3float [[cc2]] [[cc2]]
// CHECK-NEXT: OpStore %k [[cc3]]
    k = val;
}
