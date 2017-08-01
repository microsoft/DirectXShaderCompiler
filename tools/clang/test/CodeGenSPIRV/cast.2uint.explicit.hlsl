// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2uint_1_1:%\d+]] = OpConstantComposite %v2uint %uint_1 %uint_1
// CHECK: [[v2uint_0_0:%\d+]] = OpConstantComposite %v2uint %uint_0 %uint_0
// CHECK: [[v2uint_2_3:%\d+]] = OpConstantComposite %v2uint %uint_2 %uint_3

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    uint i;
    int from1;
    bool from2;
    float from3;

    uint1 vi1;
    uint2 vi2;
    uint3 vi3;
    int1 vfrom1;
    bool2 vfrom2;
    float3 vfrom3;

    // C style cast

    // From constant (explicit)
// CHECK: OpStore %i %uint_1
    i = (uint)true;
// CHECK-NEXT: OpStore %i %uint_3
    i = (uint)3.0;

    // From constant expr
// CHECK-NEXT: OpStore %i %uint_2
    i = (uint)(5.5 - 3.4);

    // From variable (explicit)
// CHECK-NEXT: [[from1:%\d+]] = OpLoad %int %from1
// CHECK-NEXT: [[c1:%\d+]] = OpBitcast %uint [[from1]]
// CHECK-NEXT: OpStore %i [[c1]]
    i = (uint)from1;
// CHECK-NEXT: [[from2:%\d+]] = OpLoad %bool %from2
// CHECK-NEXT: [[c2:%\d+]] = OpSelect %uint [[from2]] %uint_1 %uint_0
// CHECK-NEXT: OpStore %i [[c2]]
    i = (uint)from2;
// CHECK-NEXT: [[from3:%\d+]] = OpLoad %float %from3
// CHECK-NEXT: [[c3:%\d+]] = OpConvertFToU %uint [[from3]]
// CHECK-NEXT: OpStore %i [[c3]]
    i = (uint)from3;

    // C++ function style cast

// CHECK-NEXT: OpStore %i %uint_0
    i = uint(false);
// CHECK-NEXT: OpStore %i %uint_3
    i = uint(3.5);

// CHECK-NEXT: OpStore %i %uint_5
    i = uint(3.3 + 2.2);

// CHECK-NEXT: [[from4:%\d+]] = OpLoad %int %from1
// CHECK-NEXT: [[c4:%\d+]] = OpBitcast %uint [[from4]]
// CHECK-NEXT: OpStore %i [[c4]]
    i = uint(from1);
// CHECK-NEXT: [[from5:%\d+]] = OpLoad %bool %from2
// CHECK-NEXT: [[c5:%\d+]] = OpSelect %uint [[from5]] %uint_1 %uint_0
// CHECK-NEXT: OpStore %i [[c5]]
    i = uint(from2);
// CHECK-NEXT: [[from6:%\d+]] = OpLoad %float %from3
// CHECK-NEXT: [[c6:%\d+]] = OpConvertFToU %uint [[from6]]
// CHECK-NEXT: OpStore %i [[c6]]
    i = uint(from3);

    // Vector cases

// CHECK-NEXT: OpStore %vi1 %uint_3
    vi1 = (uint1)3.6;
// CHECK-NEXT: [[vfrom1:%\d+]] = OpLoad %int %vfrom1
// CHECK-NEXT: [[vc1:%\d+]] = OpBitcast %uint [[vfrom1]]
// CHECK-NEXT: OpStore %vi1 [[vc1]]
    vi1 = (uint1)vfrom1;
// CHECK-NEXT: [[vfrom2:%\d+]] = OpLoad %v2bool %vfrom2
// CHECK-NEXT: [[vc2:%\d+]] = OpSelect %v2uint [[vfrom2]] [[v2uint_1_1]] [[v2uint_0_0]]
// CHECK-NEXT: OpStore %vi2 [[vc2]]
    vi2 = (uint2)vfrom2;
// CHECK-NEXT: [[vfrom3:%\d+]] = OpLoad %v3float %vfrom3
// CHECK-NEXT: [[vc3:%\d+]] = OpConvertFToU %v3uint [[vfrom3]]
// CHECK-NEXT: OpStore %vi3 [[vc3]]
    vi3 = (uint3)vfrom3;

// CHECK-NEXT: OpStore %vi1 %uint_3
    vi1 = uint1(3.5);
// CHECK-NEXT: OpStore %vi2 [[v2uint_2_3]]
    vi2 = uint2(1.1 + 1.2, 3);
// CHECK-NEXT: [[vfrom4:%\d+]] = OpLoad %int %vfrom1
// CHECK-NEXT: [[vc4:%\d+]] = OpBitcast %uint [[vfrom4]]
// CHECK-NEXT: OpStore %vi1 [[vc4]]
    vi1 = uint1(vfrom1);
// CHECK-NEXT: [[vfrom5:%\d+]] = OpLoad %v2bool %vfrom2
// CHECK-NEXT: [[vc5:%\d+]] = OpSelect %v2uint [[vfrom5]] [[v2uint_1_1]] [[v2uint_0_0]]
// CHECK-NEXT: OpStore %vi2 [[vc5]]
    vi2 = uint2(vfrom2);
// CHECK-NEXT: [[vfrom6:%\d+]] = OpLoad %v3float %vfrom3
// CHECK-NEXT: [[vc6:%\d+]] = OpConvertFToU %v3uint [[vfrom6]]
// CHECK-NEXT: OpStore %vi3 [[vc6]]
    vi3 = uint3(vfrom3);
}

