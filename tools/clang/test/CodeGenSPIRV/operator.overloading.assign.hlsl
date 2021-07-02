// Run: %dxc -T ps_6_0 -E main -enable-operator-overloading

// CHECK:           %a = OpVariable %_ptr_Function_Number Function
// CHECK:           %b = OpVariable %_ptr_Function_Number Function
// CHECK: %param_var_x = OpVariable %_ptr_Function_Number Function
// CHECK:   [[a:%\w+]] = OpLoad %Number %a
// CHECK:                OpStore %param_var_x [[a]]
// CHECK:                OpFunctionCall %void %Number_operator_Equal %a %param_var_x

// CHECK: %Number_operator_Equal = OpFunction %void None
// CHECK:  %param_this = OpFunctionParameter %_ptr_Function_Number
// CHECK:           %x = OpFunctionParameter %_ptr_Function_Number
// CHECK: [[p_x_n:%\w+]] = OpAccessChain %_ptr_Function_int %x %int_0
// CHECK: [[x_n:%\w+]] = OpLoad %int [[p_x_n]]
// CHECK: [[p_n:%\w+]] = OpAccessChain %_ptr_Function_int %param_this %int_0
// CHECK:                OpStore [[p_n]] [[x_n]]

struct Number {
    int n;

    void operator=(Number x) {
        n = x.n;
    }
};

int main(float4 pos: SV_Position) : SV_Target {
    Number a = {pos.x};
    Number b = {pos.y};
    a = b;
    return a.n;
}
