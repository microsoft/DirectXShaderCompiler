// RUN: %dxc -T vs_6_2 -E main -enable-16bit-types

// CHECK: OpCapability Float16
// CHECK: OpCapability Int16

void main() {
    float16_t result;
    float16_t4 result4;

    // CHECK:      [[a:%\d+]] = OpLoad %short %a
    // CHECK-NEXT: [[a_as_half:%\d+]] = OpBitcast %half [[a]]
    // CHECK-NEXT: OpStore %result [[a_as_half]]
    int16_t a;
    result = asfloat16(a);

    // CHECK-NEXT: [[b:%\d+]] = OpLoad %ushort %b
    // CHECK-NEXT: [[b_as_half:%\d+]] = OpBitcast %half [[b]]
    // CHECK-NEXT: OpStore %result [[b_as_half]]
    uint16_t b;
    result = asfloat16(b);

    // CHECK-NEXT: [[c:%\d+]] = OpLoad %half %c
    // CHECK-NEXT: OpStore %result [[c]]
    float16_t c;
    result = asfloat16(c);

    // CHECK-NEXT: [[d:%\d+]] = OpLoad %v4short %d
    // CHECK-NEXT: [[d_as_half:%\d+]] = OpBitcast %v4half [[d]]
    // CHECK-NEXT: OpStore %result4 [[d_as_half]]
    int16_t4 d;
    result4 = asfloat16(d);

    // CHECK-NEXT: [[e:%\d+]] = OpLoad %v4ushort %e
    // CHECK-NEXT: [[e_as_half:%\d+]] = OpBitcast %v4half [[e]]
    // CHECK-NEXT: OpStore %result4 [[e_as_half]]
    uint16_t4 e;
    result4 = asfloat16(e);

    // CHECK-NEXT: [[f:%\d+]] = OpLoad %v4half %f
    // CHECK-NEXT: OpStore %result4 [[f]]
    float16_t4 f;
    result4 = asfloat16(f);

    int16_t2x3 intMat;
    uint16_t2x3 uintMat;

    // CHECK:       [[intMat:%\d+]] = OpLoad %_arr_v3short_uint_2 %intMat
    // CHECK-NEXT: [[intMat0:%\d+]] = OpCompositeExtract %v3short [[intMat]] 0
    // CHECK-NEXT:      [[row0:%\d+]] = OpBitcast %v3half [[intMat0]]
    // CHECK-NEXT: [[intMat1:%\d+]] = OpCompositeExtract %v3short [[intMat]] 1
    // CHECK-NEXT:      [[row1:%\d+]] = OpBitcast %v3half [[intMat1]]
    // CHECK-NEXT:         [[g:%\d+]] = OpCompositeConstruct %mat2v3half [[row0]] [[row1]]
    // CHECK-NEXT:                      OpStore %g [[g]]
    float16_t2x3 g = asfloat16(intMat);

    // CHECK:       [[uintMat:%\d+]] = OpLoad %_arr_v3ushort_uint_2 %uintMat
    // CHECK-NEXT: [[uintMat0:%\d+]] = OpCompositeExtract %v3ushort [[uintMat]] 0
    // CHECK-NEXT:      [[row0:%\d+]] = OpBitcast %v3half [[uintMat0]]
    // CHECK-NEXT: [[uintMat1:%\d+]] = OpCompositeExtract %v3ushort [[uintMat]] 1
    // CHECK-NEXT:      [[row1:%\d+]] = OpBitcast %v3half [[uintMat1]]
    // CHECK-NEXT:         [[h:%\d+]] = OpCompositeConstruct %mat2v3half [[row0]] [[row1]]
    // CHECK-NEXT:                      OpStore %h [[h]]
    float16_t2x3 h = asfloat16(uintMat);
}
