// Run: %dxc -T vs_6_0 -E main

struct S {
    bool a : A;
    uint2 b: B;
    float2x3 c: C;
};

struct T {
    S x;
    int y: D;
};

// CHECK:       %in_var_A = OpVariable %_ptr_Input_bool Input
// CHECK-NEXT:  %in_var_B = OpVariable %_ptr_Input_v2uint Input
// CHECK-NEXT:  %in_var_C = OpVariable %_ptr_Input_mat2v3float Input
// CHECK-NEXT:  %in_var_D = OpVariable %_ptr_Input_int Input

// CHECK-NEXT: %out_var_A = OpVariable %_ptr_Output_bool Output
// CHECK-NEXT: %out_var_B = OpVariable %_ptr_Output_v2uint Output
// CHECK-NEXT: %out_var_C = OpVariable %_ptr_Output_mat2v3float Output
// CHECK-NEXT: %out_var_D = OpVariable %_ptr_Output_int Output

// CHECK-NEXT:        %main = OpFunction %void None {{%\d+}}
// CHECK-NEXT:     {{%\d+}} = OpLabel

// CHECK-NEXT: %param_var_input = OpVariable %_ptr_Function_T Function
// CHECK-NEXT: [[inA:%\d+]] = OpLoad %bool %in_var_A
// CHECK-NEXT: [[inB:%\d+]] = OpLoad %v2uint %in_var_B
// CHECK-NEXT: [[inC:%\d+]] = OpLoad %mat2v3float %in_var_C
// CHECK-NEXT: [[inS:%\d+]] = OpCompositeConstruct %S [[inA]] [[inB]] [[inC]]
// CHECK-NEXT: [[inD:%\d+]] = OpLoad %int %in_var_D
// CHECK-NEXT: [[inT:%\d+]] = OpCompositeConstruct %T [[inS]] [[inD]]
// CHECK-NEXT:                OpStore %param_var_input [[inT]]

// CHECK-NEXT:  [[ret:%\d+]] = OpFunctionCall %T %src_main %param_var_input
// CHECK-NEXT: [[outS:%\d+]] = OpCompositeExtract %S [[ret]] 0
// CHECK-NEXT: [[outA:%\d+]] = OpCompositeExtract %bool [[outS]] 0
// CHECK-NEXT:                 OpStore %out_var_A [[outA]]
// CHECK-NEXT: [[outB:%\d+]] = OpCompositeExtract %v2uint [[outS]] 1
// CHECK-NEXT:                 OpStore %out_var_B [[outB]]
// CHECK-NEXT: [[outC:%\d+]] = OpCompositeExtract %mat2v3float [[outS]] 2
// CHECK-NEXT:                 OpStore %out_var_C [[outC]]
// CHECK-NEXT: [[outD:%\d+]] = OpCompositeExtract %int [[ret]] 1
// CHECK-NEXT:                 OpStore %out_var_D [[outD]]

// CHECK-NEXT:                 OpReturn
// CHECK-NEXT:                 OpFunctionEnd
T main(T input) {
    return input;
}
