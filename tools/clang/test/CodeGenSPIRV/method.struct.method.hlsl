// Run: %dxc -T vs_6_0 -E main

struct S {
    float    a;
    float3   b;

    // Not referencing members
    float fn_no_ref() {
        return 4.2;
    }

    // Referencing members
    float fn_ref() {
        return a + this.b[0];
    }

    // Calling a method defined later
    float fn_call(float c) {
        return fn_param(c);
    }

    // Passing in parameters
    float fn_param(float c) {
        return a + c;
    }

    // Unused method
    float fn_unused() {
        return 2.4;
    }

    // Static method
    static float fn_static() {
        return 3.5;
    }
};

struct T {
    S s;

    // Calling method in nested struct
    float fn_nested() {
        return s.fn_ref();
    }

    // Static method with the same name as S
    static float fn_static() {
        return 6.7;
    }
};

// CHECK:   [[ft_f32:%\d+]] = OpTypeFunction %float
// CHECK:     [[ft_S:%\d+]] = OpTypeFunction %float %_ptr_Function_S
// CHECK: [[ft_S_f32:%\d+]] = OpTypeFunction %float %_ptr_Function_S %_ptr_Function_float
// CHECK:     [[ft_T:%\d+]] = OpTypeFunction %float %_ptr_Function_T

// CHECK-LABEL:   %src_main = OpFunction
// CHECK-NEXT:    %bb_entry = OpLabel
// CHECK-NEXT:           %s = OpVariable %_ptr_Function_S Function
// CHECK-NEXT:           %t = OpVariable %_ptr_Function_T Function
// CHECK-NEXT: %param_var_c = OpVariable %_ptr_Function_float Function
// CHECK:          {{%\d+}} = OpFunctionCall %float %S_fn_no_ref %s
// CHECK:          {{%\d+}} = OpFunctionCall %float %S_fn_ref %s
// CHECK:          {{%\d+}} = OpFunctionCall %float %S_fn_call %s %param_var_c
// CHECK:          {{%\d+}} = OpFunctionCall %float %T_fn_nested %t
// CHECK:          {{%\d+}} = OpFunctionCall %float %S_fn_static
// CHECK:          {{%\d+}} = OpFunctionCall %float %T_fn_static
// CHECK:          {{%\d+}} = OpFunctionCall %float %S_fn_static
// CHECK:          {{%\d+}} = OpFunctionCall %float %T_fn_static
// CHECK:                     OpFunctionEnd
float main() : A {
    S s;
    T t;
    return s.fn_no_ref() + s.fn_ref() + s.fn_call(5.0) + t.fn_nested() +
           s.fn_static() + t.fn_static() + S::fn_static() + T::fn_static();
}

// CHECK:       %S_fn_no_ref = OpFunction %float None [[ft_S]]
// CHECK-NEXT:   %param_this = OpFunctionParameter %_ptr_Function_S
// CHECK-NEXT:   %bb_entry_0 = OpLabel
// CHECK:                      OpFunctionEnd


// CHECK:          %S_fn_ref = OpFunction %float None [[ft_S]]
// CHECK-NEXT: %param_this_0 = OpFunctionParameter %_ptr_Function_S
// CHECK-NEXT:   %bb_entry_1 = OpLabel
// CHECK:           {{%\d+}} = OpAccessChain %_ptr_Function_float %param_this_0 %int_0
// CHECK:           {{%\d+}} = OpAccessChain %_ptr_Function_float %param_this_0 %int_1 %uint_0
// CHECK:                      OpFunctionEnd


// CHECK:          %S_fn_call = OpFunction %float None [[ft_S_f32]]
// CHECK-NEXT:  %param_this_1 = OpFunctionParameter %_ptr_Function_S
// CHECK-NEXT:             %c = OpFunctionParameter %_ptr_Function_float
// CHECK-NEXT:    %bb_entry_2 = OpLabel
// CHECK-NEXT: %param_var_c_0 = OpVariable %_ptr_Function_float Function
// CHECK:            {{%\d+}} = OpFunctionCall %float %S_fn_param %param_this_1 %param_var_c_0
// CHECK:                       OpFunctionEnd

// CHECK:       %T_fn_nested = OpFunction %float None [[ft_T]]
// CHECK-NEXT: %param_this_2 = OpFunctionParameter %_ptr_Function_T
// CHECK-NEXT:   %bb_entry_3 = OpLabel
// CHECK:       [[t_s:%\d+]] = OpAccessChain %_ptr_Function_S %param_this_2 %int_0
// CHECK:           {{%\d+}} = OpFunctionCall %float %S_fn_ref [[t_s]]
// CHECK:                      OpFunctionEnd


// CHECK:        %S_fn_static = OpFunction %float None [[ft_f32]]
// CHECK-NEXT:    %bb_entry_4 = OpLabel
// CHECK:                       OpFunctionEnd


// CHECK:        %T_fn_static = OpFunction %float None [[ft_f32]]
// CHECK-NEXT:    %bb_entry_5 = OpLabel
// CHECK:                       OpFunctionEnd


// CHECK:        %S_fn_param = OpFunction %float None [[ft_S_f32]]
// CHECK-NEXT: %param_this_3 = OpFunctionParameter %_ptr_Function_S
// CHECK-NEXT:          %c_0 = OpFunctionParameter %_ptr_Function_float
// CHECK-NEXT:   %bb_entry_6 = OpLabel
// CHECK:           {{%\d+}} = OpAccessChain %_ptr_Function_float %param_this_3 %int_0
// CHECK:                      OpFunctionEnd
