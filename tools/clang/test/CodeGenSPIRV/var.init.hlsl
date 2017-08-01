// Run: %dxc -T ps_6_0 -E main

// Constants
// CHECK-DAG: %float_1 = OpConstant %float 1
// CHECK-DAG: %float_2 = OpConstant %float 2
// CHECK-DAG: %float_3 = OpConstant %float 3
// CHECK-DAG: %float_4 = OpConstant %float 4
// CHECK-DAG: %int_1 = OpConstant %int 1
// CHECK-DAG: %int_2 = OpConstant %int 2
// CHECK-DAG: [[float4constant:%\d+]] = OpConstantComposite %v4float %float_1 %float_2 %float_3 %float_4
// CHECK-DAG: [[int2constant:%\d+]] = OpConstantComposite %v2int %int_1 %int_2

// Stage IO variables
// CHECK-DAG: [[component:%\d+]] = OpVariable %_ptr_Input_float Input

float4 main(float component: COLOR) : SV_TARGET {
// CHECK-LABEL: %bb_entry = OpLabel

// CHECK-NEXT: %a = OpVariable %_ptr_Function_int Function %int_0
// CHECK-NEXT: %b = OpVariable %_ptr_Function_int Function

// CHECK-NEXT: %i = OpVariable %_ptr_Function_float Function %float_3
// CHECK-NEXT: %j = OpVariable %_ptr_Function_float Function

// CHECK-NEXT: %m = OpVariable %_ptr_Function_v4float Function
// CHECK-NEXT: %n = OpVariable %_ptr_Function_v4float Function
// CHECK-NEXT: %o = OpVariable %_ptr_Function_v4float Function

// CHECK-NEXT: %p = OpVariable %_ptr_Function_v2int Function [[int2constant]]
// CHECK-NEXT: %q = OpVariable %_ptr_Function_v3int Function

// CHECK-NEXT: %x = OpVariable %_ptr_Function_uint Function

// Initializer already attached to the var definition
    int a = 0; // From constant
// CHECK-NEXT: [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: OpStore %b [[a0]]
    int b = a; // From local variable

// Initializer already attached to the var definition
    float i = 1. + 2.;   // From const expr
// CHECK-NEXT: [[component0:%\d+]] = OpLoad %float [[component]]
// CHECK-NEXT: OpStore %j [[component0]]
    float j = component; // From stage variable

// CHECK-NEXT: OpStore %m [[float4constant]]
    float4 m = float4(1.0, 2.0, 3.0, 4.0);  // All components are constants
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %float %j
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %float %j
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %float %j
// CHECK-NEXT: [[j3:%\d+]] = OpLoad %float %j
// CHECK-NEXT: [[ninit:%\d+]] = OpCompositeConstruct %v4float [[j0]] [[j1]] [[j2]] [[j3]]
// CHECK-NEXT: OpStore %n [[ninit]]
    float4 n = float4(j, j, j, j);          // All components are variables
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %float %j
// CHECK-NEXT: [[j5:%\d+]] = OpLoad %float %j
// CHECK-NEXT: [[oinit:%\d+]] = OpCompositeConstruct %v4float %float_1 [[j4]] %float_3 [[j5]]
// CHECK-NEXT: OpStore %o [[oinit]]
    float4 o = float4(1.0, j, 3.0, j);      // Mixed case

    int2 p = {1, 2}; // All components are constants
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[qinit:%\d+]] = OpCompositeConstruct %v3int %int_4 [[b1]] [[a1]]
// CHECK-NEXT: OpStore %q [[qinit]]
    int3 q = {4, b, a}; // Mixed cases

// CHECK-NEXT: OpStore %x %uint_1
    uint1 x = uint1(1); // Special case: vector of size 1

    return o;
}
