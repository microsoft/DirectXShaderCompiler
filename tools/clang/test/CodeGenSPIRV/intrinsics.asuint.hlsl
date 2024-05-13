// RUN: %dxc -T vs_6_0 -E main -fcgl  %s -spirv | FileCheck %s

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

    // CHECK:      [[b:%[0-9]+]] = OpLoad %int %b
    // CHECK-NEXT: [[b_as_uint:%[0-9]+]] = OpBitcast %uint [[b]]
    // CHECK-NEXT: OpStore %result [[b_as_uint]]
    int b;
    result = asuint(b);

    // CHECK-NEXT: [[c:%[0-9]+]] = OpLoad %float %c
    // CHECK-NEXT: [[c_as_uint:%[0-9]+]] = OpBitcast %uint [[c]]
    // CHECK-NEXT: OpStore %result [[c_as_uint]]
    float c;
    result = asuint(c);

    // CHECK-NEXT: [[e:%[0-9]+]] = OpLoad %int %e
    // CHECK-NEXT: [[e_as_uint:%[0-9]+]] = OpBitcast %uint [[e]]
    // CHECK-NEXT: OpStore %result [[e_as_uint]]
    int1 e;
    result = asuint(e);

    // CHECK-NEXT: [[f:%[0-9]+]] = OpLoad %float %f
    // CHECK-NEXT: [[f_as_uint:%[0-9]+]] = OpBitcast %uint [[f]]
    // CHECK-NEXT: OpStore %result [[f_as_uint]]
    float1 f;
    result = asuint(f);

    // CHECK-NEXT: [[h:%[0-9]+]] = OpLoad %v4int %h
    // CHECK-NEXT: [[h_as_uint:%[0-9]+]] = OpBitcast %v4uint [[h]]
    // CHECK-NEXT: OpStore %result4 [[h_as_uint]]
    int4 h;
    result4 = asuint(h);

    // CHECK-NEXT: [[i:%[0-9]+]] = OpLoad %v4float %i
    // CHECK-NEXT: [[i_as_uint:%[0-9]+]] = OpBitcast %v4uint [[i]]
    // CHECK-NEXT: OpStore %result4 [[i_as_uint]]
    float4 i;
    result4 = asuint(i);

    float2x3 floatMat;
    int2x3 intMat;
    
// CHECK:       [[floatMat:%[0-9]+]] = OpLoad %mat2v3float %floatMat
// CHECK-NEXT: [[floatMat0:%[0-9]+]] = OpCompositeExtract %v3float [[floatMat]] 0
// CHECK-NEXT:      [[row0:%[0-9]+]] = OpBitcast %v3uint [[floatMat0]]
// CHECK-NEXT: [[floatMat1:%[0-9]+]] = OpCompositeExtract %v3float [[floatMat]] 1
// CHECK-NEXT:      [[row1:%[0-9]+]] = OpBitcast %v3uint [[floatMat1]]
// CHECK-NEXT:         [[j:%[0-9]+]] = OpCompositeConstruct %_arr_v3uint_uint_2 [[row0]] [[row1]]
// CHECK-NEXT:                      OpStore %j [[j]]
    uint2x3 j = asuint(floatMat);
// CHECK:       [[intMat:%[0-9]+]] = OpLoad %_arr_v3int_uint_2 %intMat
// CHECK-NEXT: [[intMat0:%[0-9]+]] = OpCompositeExtract %v3int [[intMat]] 0
// CHECK-NEXT:    [[row0_0:%[0-9]+]] = OpBitcast %v3uint [[intMat0]]
// CHECK-NEXT: [[intMat1:%[0-9]+]] = OpCompositeExtract %v3int [[intMat]] 1
// CHECK-NEXT:    [[row1_0:%[0-9]+]] = OpBitcast %v3uint [[intMat1]]
// CHECK-NEXT:       [[k:%[0-9]+]] = OpCompositeConstruct %_arr_v3uint_uint_2 [[row0_0]] [[row1_0]]
// CHECK-NEXT:                    OpStore %k [[k]]
    uint2x3 k = asuint(intMat);

    double value;
    uint lowbits;
    uint highbits;
// CHECK-NEXT:      [[value:%[0-9]+]] = OpLoad %double %value
// CHECK-NEXT:  [[resultVec:%[0-9]+]] = OpBitcast %v2uint [[value]]
// CHECK-NEXT: [[resultVec0:%[0-9]+]] = OpCompositeExtract %uint [[resultVec]] 0
// CHECK-NEXT:                       OpStore %lowbits [[resultVec0]]
// CHECK-NEXT: [[resultVec1:%[0-9]+]] = OpCompositeExtract %uint [[resultVec]] 1
// CHECK-NEXT:                       OpStore %highbits [[resultVec1]]
    asuint(value, lowbits, highbits);
}
