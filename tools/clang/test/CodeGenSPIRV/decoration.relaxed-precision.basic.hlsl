// Run: %dxc -T ps_6_0 -E main

// CHECK:                      OpDecorate %in_var_TEXCOORD0 RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %out_var_SV_Target RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %param_var_x RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[coordValue:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[mainResult:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %src_main RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %x RelaxedPrecision
// CHECK-NEXT:                 OpDecorate %y RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[xValue1:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[comparison:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[xValue2:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[xValue3:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[xMulx:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[xValue4:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[yValue1:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[yValue2:%\d+]] RelaxedPrecision
// CHECK-NEXT:                 OpDecorate [[yMuly:%\d+]] RelaxedPrecision

// CHECK:  %in_var_TEXCOORD0 = OpVariable %_ptr_Input_float Input
// CHECK: %out_var_SV_Target = OpVariable %_ptr_Output_float Output
// CHECK:       %param_var_x = OpVariable %_ptr_Function_float Function

// CHECK:     [[coordValue]] = OpLoad %float %in_var_TEXCOORD0
// CHECK:     [[mainResult]] = OpFunctionCall %float %src_main %param_var_x
min16float main(min16float x : TEXCOORD0) : SV_Target {
// CHECK:          %src_main = OpFunction %float None {{%\d+}}
// CHECK:                 %x = OpFunctionParameter %_ptr_Function_float
// CHECK:                 %y = OpVariable %_ptr_Function_float Function
// CHECK:        [[xValue1]] = OpLoad %float %x
// CHECK:     [[comparison]] = OpFOrdGreaterThan %bool [[xValue1]] %float_0
    min16float y;
    if (x > 0) {
// CHECK:        [[xValue2]] = OpLoad %float %x
// CHECK:        [[xValue3]] = OpLoad %float %x
// CHECK:          [[xMulx]] = OpFMul %float [[xValue2]] [[xValue3]]
        y = x * x;
    }
    else {
// CEHCK:        [[xValue4]] = OpLoad %float %x
        y = x;
    }
// CHECK:        [[yValue1]] = OpLoad %float %y
// CHECK:        [[yValue2]] = OpLoad %float %y
// CHECK:          [[yMuly]] = OpFMul %float [[yValue1]] [[yValue2]]
    return y * y;
}
