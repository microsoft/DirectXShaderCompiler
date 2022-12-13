// RUN: %dxc -T vs_6_2 -E main -enable-16bit-types

// CHECK: OpCapability Int16
// CHECK: OpCapability Float16

void main() {
    int16_t result;
    int16_t4 result4;

    // CHECK: [[a:%\d+]] = OpLoad %short %a
    // CHECK-NEXT: OpStore %result [[a]]
    int16_t a;
    result = asint16(a);

    // CHECK: [[b:%\d+]] = OpLoad %ushort %b
    // CHECK-NEXT: [[b_as_short:%\d+]] = OpBitcast %short [[b]]
    // CHECK-NEXT: OpStore %result [[b_as_short]]
    uint16_t b;
    result = asint16(b);

    // CHECK: [[c:%\d+]] = OpLoad %half %c
    // CHECK-NEXT: [[c_as_short:%\d+]] = OpBitcast %short [[c]]
    // CHECK-NEXT: OpStore %result [[c_as_short]]
    float16_t c;
    result = asint16(c);

    // CHECK: [[d:%\d+]] = OpLoad %v4short %d
    // CHECK-NEXT: OpStore %result4 [[d]]
    int16_t4 d;
    result4 = asint16(d);

    // CHECK: [[e:%\d+]] = OpLoad %v4ushort %e
    // CHECK-NEXT: [[e_as_short:%\d+]] = OpBitcast %v4short [[e]]
    // CHECK-NEXT: OpStore %result4 [[e_as_short]]
    uint16_t4 e;
    result4 = asint16(e);

    // CHECK: [[f:%\d+]] = OpLoad %v4half %f
    // CHECK-NEXT: [[f_as_short:%\d+]] = OpBitcast %v4short [[f]]
    // CHECK-NEXT: OpStore %result4 [[f_as_short]]
    float16_t4 f;
    result4 = asint16(f);

    float16_t2x3 floatMat;
    uint16_t2x3 uintMat;

    // CHECK:       [[floatMat:%\d+]] = OpLoad %mat2v3half %floatMat
    // CHECK-NEXT: [[floatMat0:%\d+]] = OpCompositeExtract %v3half [[floatMat]] 0
    // CHECK-NEXT:      [[row0:%\d+]] = OpBitcast %v3short [[floatMat0]]
    // CHECK-NEXT: [[floatMat1:%\d+]] = OpCompositeExtract %v3half [[floatMat]] 1
    // CHECK-NEXT:      [[row1:%\d+]] = OpBitcast %v3short [[floatMat1]]
    // CHECK-NEXT:         [[g:%\d+]] = OpCompositeConstruct %_arr_v3short_uint_2 [[row0]] [[row1]]
    // CHECK-NEXT:                      OpStore %g [[g]]
    int16_t2x3 g = asint16(floatMat);

    // CHECK:       [[uintMat:%\d+]] = OpLoad %_arr_v3ushort_uint_2 %uintMat
    // CHECK-NEXT: [[uintMat0:%\d+]] = OpCompositeExtract %v3ushort [[uintMat]] 0
    // CHECK-NEXT:      [[row0:%\d+]] = OpBitcast %v3short [[uintMat0]]
    // CHECK-NEXT: [[uintMat1:%\d+]] = OpCompositeExtract %v3ushort [[uintMat]] 1
    // CHECK-NEXT:      [[row1:%\d+]] = OpBitcast %v3short [[uintMat1]]
    // CHECK-NEXT:         [[h:%\d+]] = OpCompositeConstruct %_arr_v3short_uint_2 [[row0]] [[row1]]
    // CHECK-NEXT:                      OpStore %h [[h]]
    int16_t2x3 h = asint16(uintMat);
}
