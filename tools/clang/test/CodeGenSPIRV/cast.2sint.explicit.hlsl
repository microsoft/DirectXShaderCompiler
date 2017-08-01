// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2int_1_1:%\d+]] = OpConstantComposite %v2int %int_1 %int_1
// CHECK: [[v2int_0_0:%\d+]] = OpConstantComposite %v2int %int_0 %int_0
// CHECK: [[v2int_2_n3:%\d+]] = OpConstantComposite %v2int %int_2 %int_n3

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    int i;
    uint from1;
    bool from2;
    float from3;

    int1 vi1;
    int2 vi2;
    int3 vi3;
    uint1 vfrom1;
    bool2 vfrom2;
    float3 vfrom3;

    // C style cast

    // From constant (explicit)
// CHECK: OpStore %i %int_1
    i = (int)true;
// CHECK-NEXT: OpStore %i %int_3
    i = (int)3.0;

    // From constant expr
// CHECK-NEXT: OpStore %i %int_n2
    i = (int)(3.4 - 5.5);

    // From variable (explicit)
// CHECK-NEXT: [[from1:%\d+]] = OpLoad %uint %from1
// CHECK-NEXT: [[c1:%\d+]] = OpBitcast %int [[from1]]
// CHECK-NEXT: OpStore %i [[c1]]
    i = (int)from1;
// CHECK-NEXT: [[from2:%\d+]] = OpLoad %bool %from2
// CHECK-NEXT: [[c2:%\d+]] = OpSelect %int [[from2]] %int_1 %int_0
// CHECK-NEXT: OpStore %i [[c2]]
    i = (int)from2;
// CHECK-NEXT: [[from3:%\d+]] = OpLoad %float %from3
// CHECK-NEXT: [[c3:%\d+]] = OpConvertFToS %int [[from3]]
// CHECK-NEXT: OpStore %i [[c3]]
    i = (int)from3;

    // C++ function style cast

// CHECK-NEXT: OpStore %i %int_0
    i = int(false);
// CHECK-NEXT: OpStore %i %int_3
    i = int(3.5);

// CHECK-NEXT: OpStore %i %int_5
    i = int(3.3 + 2.2);

// CHECK-NEXT: [[from4:%\d+]] = OpLoad %uint %from1
// CHECK-NEXT: [[c4:%\d+]] = OpBitcast %int [[from4]]
// CHECK-NEXT: OpStore %i [[c4]]
    i = int(from1);
// CHECK-NEXT: [[from5:%\d+]] = OpLoad %bool %from2
// CHECK-NEXT: [[c5:%\d+]] = OpSelect %int [[from5]] %int_1 %int_0
// CHECK-NEXT: OpStore %i [[c5]]
    i = int(from2);
// CHECK-NEXT: [[from6:%\d+]] = OpLoad %float %from3
// CHECK-NEXT: [[c6:%\d+]] = OpConvertFToS %int [[from6]]
// CHECK-NEXT: OpStore %i [[c6]]
    i = int(from3);

    // Vector cases

// CHECK-NEXT: OpStore %vi1 %int_3
    vi1 = (int1)3.6;
// CHECK-NEXT: [[vfrom1:%\d+]] = OpLoad %uint %vfrom1
// CHECK-NEXT: [[vc1:%\d+]] = OpBitcast %int [[vfrom1]]
// CHECK-NEXT: OpStore %vi1 [[vc1]]
    vi1 = (int1)vfrom1;
// CHECK-NEXT: [[vfrom2:%\d+]] = OpLoad %v2bool %vfrom2
// CHECK-NEXT: [[vc2:%\d+]] = OpSelect %v2int [[vfrom2]] [[v2int_1_1]] [[v2int_0_0]]
// CHECK-NEXT: OpStore %vi2 [[vc2]]
    vi2 = (int2)vfrom2;
// CHECK-NEXT: [[vfrom3:%\d+]] = OpLoad %v3float %vfrom3
// CHECK-NEXT: [[vc3:%\d+]] = OpConvertFToS %v3int [[vfrom3]]
// CHECK-NEXT: OpStore %vi3 [[vc3]]
    vi3 = (int3)vfrom3;

// CHECK-NEXT: OpStore %vi1 %int_3
    vi1 = int1(3.5);
// CHECK-NEXT: OpStore %vi2 [[v2int_2_n3]]
    vi2 = int2(1.1 + 1.2, -3);
// CHECK-NEXT: [[vfrom4:%\d+]] = OpLoad %uint %vfrom1
// CHECK-NEXT: [[vc4:%\d+]] = OpBitcast %int [[vfrom4]]
// CHECK-NEXT: OpStore %vi1 [[vc4]]
    vi1 = int1(vfrom1);
// CHECK-NEXT: [[vfrom5:%\d+]] = OpLoad %v2bool %vfrom2
// CHECK-NEXT: [[vc5:%\d+]] = OpSelect %v2int [[vfrom5]] [[v2int_1_1]] [[v2int_0_0]]
// CHECK-NEXT: OpStore %vi2 [[vc5]]
    vi2 = int2(vfrom2);
// CHECK-NEXT: [[vfrom6:%\d+]] = OpLoad %v3float %vfrom3
// CHECK-NEXT: [[vc6:%\d+]] = OpConvertFToS %v3int [[vfrom6]]
// CHECK-NEXT: OpStore %vi3 [[vc6]]
    vi3 = int3(vfrom3);
}
