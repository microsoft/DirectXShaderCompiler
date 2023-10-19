// RUN: %dxc -T cs_6_0 -E main

// CHECK:                      OpDecorate %testShared RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %test RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %testTypedef RelaxedPrecision
// CHECK-NOT:                  OpDecorate %notMin16 RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %a RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[compositeConstr1:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[compositeConstr2:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[loadFromTest1:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[loadFromTest2:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[loadFromTestTypedef:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[multiply1:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[loadVariable1:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[multiply2:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[loadFromShared:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[loadVariable2:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[multiply3:%\d+]] RelaxedPrecision

typedef min16float ArrayOfMin16Float0[2];
typedef ArrayOfMin16Float0 ArrayOfMin16Float;

groupshared min16float testShared[2];

[numthreads(1, 1, 1)] void main()
{
// CHECK: [[compositeConstr2:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_2 {{%\d+}} {{%\d+}}
    min16float2 test[2] = { float2(1.0, 1.0), float2(1.0, 1.0) };
// CHECK: [[compositeConstr1:%\d+]] = OpCompositeConstruct %_arr_float_uint_2 %float_1 %float_1
    ArrayOfMin16Float testTypedef = { 1.0, 1.0 };
    float notMin16[2] = { 1.0, 1.0 };
// CHECK: [[loadFromTest1:%\d+]] = OpLoad %float {{%\d+}}
    testShared[0] = test[0].x;

    min16float a = 1.0;
// CHECK: [[loadFromTest2:%\d+]] = OpLoad %float {{%\d+}}
// CHECK: [[loadFromTestTypedef:%\d+]] = OpLoad %float {{%\d+}}
// CHECK: [[multiply1:%\d+]] = OpFMul %float {{%\d+}} {{%\d+}}
// CHECK: [[loadVariable1:%\d+]] = OpLoad %float %a
// CHECK: [[multiply2:%\d+]] = OpFMul %float {{%\d+}} {{%\d+}}
    a *= (test[0].x * testTypedef[0]);
// CHECK: [[loadFromShared:%\d+]] = OpLoad %float {{%\d+}}
// CHECK: [[loadVariable2:%\d+]] = OpLoad %float %a
// CHECK: [[multiply3:%\d+]] = OpFMul %float {{%\d+}} {{%\d+}}
    a *= testShared[0];
}
