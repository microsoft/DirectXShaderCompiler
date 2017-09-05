// Run: %dxc -T vs_6_0 -E main

struct X {
    int    a: A;
    float4 b: B;
};

struct Y {
    uint   c: C;
    float4 d: D;
};

struct Z {
    float e: E;
};

// CHECK:       %in_var_O = OpVariable %_ptr_Input_v4int Input
// CHECK-NEXT:  %in_var_Q = OpVariable %_ptr_Input_v4int Input
// CHECK-NEXT:  %in_var_A = OpVariable %_ptr_Input_int Input
// CHECK-NEXT:  %in_var_B = OpVariable %_ptr_Input_v4float Input
// CHECK-NEXT:  %in_var_C = OpVariable %_ptr_Input_uint Input
// CHECK-NEXT:  %in_var_D = OpVariable %_ptr_Input_v4float Input
// CHECK-NEXT:  %in_var_R = OpVariable %_ptr_Input_float Input
// CHECK-NEXT:  %in_var_E = OpVariable %_ptr_Input_float Input

// CHECK-NEXT: %out_var_P = OpVariable %_ptr_Output_v4int Output
// CHECK-NEXT: %out_var_Q = OpVariable %_ptr_Output_v4int Output
// CHECK-NEXT: %out_var_A = OpVariable %_ptr_Output_int Output
// CHECK-NEXT: %out_var_B = OpVariable %_ptr_Output_v4float Output
// CHECK-NEXT: %out_var_C = OpVariable %_ptr_Output_uint Output
// CHECK-NEXT: %out_var_D = OpVariable %_ptr_Output_v4float Output

// CHECK:      %main = OpFunction %void None
// CHECK-NEXT: OpLabel

// CHECK-NEXT: %param_var_param1 = OpVariable %_ptr_Function_v4int Function
// CHECK-NEXT: %param_var_param2 = OpVariable %_ptr_Function_v4int Function
// CHECK-NEXT: %param_var_param3 = OpVariable %_ptr_Function_v4int Function
// CHECK-NEXT: %param_var_param4 = OpVariable %_ptr_Function_X Function
// CHECK-NEXT: %param_var_param5 = OpVariable %_ptr_Function_X Function
// CHECK-NEXT: %param_var_param6 = OpVariable %_ptr_Function_Y Function
// CHECK-NEXT: %param_var_param7 = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %param_var_param8 = OpVariable %_ptr_Function_Z Function

// CHECK-NEXT:  [[inO:%\d+]] = OpLoad %v4int %in_var_O
// CHECK-NEXT:                 OpStore %param_var_param1 [[inO]]
// CHECK-NEXT:  [[inQ:%\d+]] = OpLoad %v4int %in_var_Q
// CHECK-NEXT:                 OpStore %param_var_param3 [[inQ]]
// CHECK-NEXT:  [[inA:%\d+]] = OpLoad %int %in_var_A
// CHECK-NEXT:  [[inB:%\d+]] = OpLoad %v4float %in_var_B
// CHECK-NEXT:  [[inX:%\d+]] = OpCompositeConstruct %X [[inA]] [[inB]]
// CHECK-NEXT:                 OpStore %param_var_param4 [[inX]]
// CHECK-NEXT:  [[inC:%\d+]] = OpLoad %uint %in_var_C
// CHECK-NEXT:  [[inD:%\d+]] = OpLoad %v4float %in_var_D
// CHECK-NEXT:  [[inY:%\d+]] = OpCompositeConstruct %Y [[inC]] [[inD]]
// CHECK-NEXT:                 OpStore %param_var_param6 [[inY]]
// CHECK-NEXT:  [[inR:%\d+]] = OpLoad %float %in_var_R
// CHECK-NEXT:                 OpStore %param_var_param7 [[inR]]
// CHECK-NEXT:  [[inE:%\d+]] = OpLoad %float %in_var_E
// CHECK-NEXT:  [[inZ:%\d+]] = OpCompositeConstruct %Z [[inE]]
// CHECK-NEXT:                 OpStore %param_var_param8 [[inZ]]

// CHECK-NEXT:                 OpFunctionCall %void %src_main %param_var_param1 %param_var_param2 %param_var_param3 %param_var_param4 %param_var_param5 %param_var_param6 %param_var_param7 %param_var_param8
// CHECK-NEXT: [[outP:%\d+]] = OpLoad %v4int %param_var_param2
// CHECK-NEXT:                 OpStore %out_var_P [[outP]]
// CHECK-NEXT: [[outQ:%\d+]] = OpLoad %v4int %param_var_param3
// CHECK-NEXT:                 OpStore %out_var_Q [[outQ]]
// CHECK-NEXT: [[outX:%\d+]] = OpLoad %X %param_var_param5
// CHECK-NEXT: [[outA:%\d+]] = OpCompositeExtract %int [[outX]] 0
// CHECK-NEXT:                 OpStore %out_var_A [[outA]]
// CHECK-NEXT: [[outB:%\d+]] = OpCompositeExtract %v4float [[outX]] 1
// CHECK-NEXT:                 OpStore %out_var_B [[outB]]
// CHECK-NEXT: [[outY:%\d+]] = OpLoad %Y %param_var_param6
// CHECK-NEXT: [[outC:%\d+]] = OpCompositeExtract %uint [[outY]] 0
// CHECK-NEXT:                 OpStore %out_var_C [[outC]]
// CHECK-NEXT: [[outD:%\d+]] = OpCompositeExtract %v4float [[outY]] 1
// CHECK-NEXT:                 OpStore %out_var_D [[outD]]

// CHECK-NEXT: OpReturn
// CHECK-NEXT: OpFunctionEnd

// Input  semantics: O, Q, A, B, C, D, R, E
// Output semantics: P, Q, A, B, C, D
void main(in      int4  param1: O,
          out     int4  param2: P,
          inout   int4  param3: Q,
          in      X     param4,
          out     X     param5,
          inout   Y     param6,
          uniform float param7: R,
          uniform Z     param8)
{
// CHECK-LABEL: %src_main = OpFunction
    param2 = param1;
    param3 = param1;

    param5 = param4;
    param6.c = param4.a;
    param6.d = param4.b;
}
