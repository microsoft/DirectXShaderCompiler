// Run: %dxc -T vs_6_0 -E main

// Signature: ret asuint(arg)
//    arg component type = {float, int}
//    arg template  type = {scalar, vector, matrix}
//    ret template  type = same as arg template type.
//    ret component type = uint

// Signature:
//           void asuint(
//           in  double value,
//           out uint lowbits,
//           out uint highbits
//           )

void main() {
    uint result;
    uint4 result4;

    // CHECK:      [[b:%\d+]] = OpLoad %int %b
    // CHECK-NEXT: [[b_as_uint:%\d+]] = OpBitcast %uint [[b]]
    // CHECK-NEXT: OpStore %result [[b_as_uint]]
    int b;
    result = asuint(b);

    // CHECK-NEXT: [[c:%\d+]] = OpLoad %float %c
    // CHECK-NEXT: [[c_as_uint:%\d+]] = OpBitcast %uint [[c]]
    // CHECK-NEXT: OpStore %result [[c_as_uint]]
    float c;
    result = asuint(c);

    // CHECK-NEXT: [[e:%\d+]] = OpLoad %int %e
    // CHECK-NEXT: [[e_as_uint:%\d+]] = OpBitcast %uint [[e]]
    // CHECK-NEXT: OpStore %result [[e_as_uint]]
    int1 e;
    result = asuint(e);

    // CHECK-NEXT: [[f:%\d+]] = OpLoad %float %f
    // CHECK-NEXT: [[f_as_uint:%\d+]] = OpBitcast %uint [[f]]
    // CHECK-NEXT: OpStore %result [[f_as_uint]]
    float1 f;
    result = asuint(f);

    // CHECK-NEXT: [[h:%\d+]] = OpLoad %v4int %h
    // CHECK-NEXT: [[h_as_uint:%\d+]] = OpBitcast %v4uint [[h]]
    // CHECK-NEXT: OpStore %result4 [[h_as_uint]]
    int4 h;
    result4 = asuint(h);

    // CHECK-NEXT: [[i:%\d+]] = OpLoad %v4float %i
    // CHECK-NEXT: [[i_as_uint:%\d+]] = OpBitcast %v4uint [[i]]
    // CHECK-NEXT: OpStore %result4 [[i_as_uint]]
    float4 i;
    result4 = asuint(i);

    double value;
    uint lowbits;
    uint highbits;
// CHECK-NEXT:      [[value:%\d+]] = OpLoad %double %value
// CHECK-NEXT:  [[resultVec:%\d+]] = OpBitcast %v2uint [[value]]
// CHECK-NEXT: [[resultVec0:%\d+]] = OpCompositeExtract %uint [[resultVec]] 0
// CHECK-NEXT:                       OpStore %lowbits [[resultVec0]]
// CHECK-NEXT: [[resultVec1:%\d+]] = OpCompositeExtract %uint [[resultVec]] 1
// CHECK-NEXT:                       OpStore %highbits [[resultVec1]]
    asuint(value, lowbits, highbits);
}
