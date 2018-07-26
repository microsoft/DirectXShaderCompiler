// Run: %dxc -T ps_6_0 -E main -fvk-use-dx-position-w

float4 main(float4 pos: SV_Position) : SV_Target {
    return pos;
}

// CHECK:       [[old:%\d+]] = OpLoad %v4float %gl_FragCoord
// CHECK-NEXT: [[oldW:%\d+]] = OpCompositeExtract %float [[old]] 3
// CHECK-NEXT: [[newW:%\d+]] = OpFDiv %float %float_1 [[oldW]]
// CHECK-NEXT:  [[new:%\d+]] = OpCompositeInsert %v4float [[newW]] [[old]] 3
// CHECK-NEXT:                 OpStore %param_var_pos [[new]]
// CHECK-NEXT:                 OpFunctionCall %v4float %src_main %param_var_pos
