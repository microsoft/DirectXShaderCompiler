// Run: %dxc -T ps_6_0 -E main

// CHECK:           %a = OpVariable %_ptr_Function_Number Function
// CHECK:           %b = OpVariable %_ptr_Function_Number Function
// CHECK:         %foo = OpVariable %_ptr_Function_int Function
// CHECK: %param_var_x = OpVariable %_ptr_Function_Number Function
// CHECK:   [[a:%\w+]] = OpLoad %Number %a
// CHECK:                OpStore %param_var_x [[a]]
// CHECK: [[call:%\w+]] = OpFunctionCall %int %Number_operator_Star %a %param_var_x
// CHECK:                OpStore %foo [[call]]

// CHECK: %Number_operator_Star = OpFunction %int None
// CHECK:  %param_this = OpFunctionParameter %_ptr_Function_Number
// CHECK:           %x = OpFunctionParameter %_ptr_Function_Number

struct Number {
    int n;

    int operator*(Number x) {
        return x.n * n;
    }
};

void main() {
    Number a, b;
    int foo = a * b;
}
