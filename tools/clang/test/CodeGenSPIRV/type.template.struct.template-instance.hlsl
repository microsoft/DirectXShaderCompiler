// RUN: %dxc -T ps_6_0 -E main -HV 2021

// The SPIR-V backend correctly handles the template instance `Foo<int>`.
// The created template instance is ClassTemplateSpecializationDecl in AST.

template <typename T>
struct Foo {
    static const T bar = 0;

    T value;
    void set(T value_) { value = value_; }
    T get() { return value; }
};

void main() {
// CHECK: [[bar_int:%\w+]] = OpVariable %_ptr_Private_int Private
// CHECK: [[bar_float:%\w+]] = OpVariable %_ptr_Private_float Private

// CHECK: OpStore [[bar_int]] %int_0
// CHECK: OpStore [[bar_float]] %float_0

    Foo<int>::bar;

// CHECK: %x = OpVariable %_ptr_Function_int Function
    int x;

// CHECK: %y = OpVariable %_ptr_Function_Foo Function
    Foo<float> y;

// CHECK:       [[x:%\w+]] = OpLoad %int %x
// CHECK: [[float_x:%\w+]] = OpConvertSToF %float [[x]]
// CHECK:                    OpStore [[param_value_:%\w+]] [[float_x]]
// CHECK:                    OpFunctionCall %void %Foo_set %y [[param_value_]]
    y.set(x);

// CHECK:     [[y_get:%\w+]] = OpFunctionCall %float %Foo_get %y
// CHECK: [[y_get_int:%\w+]] = OpConvertFToS %int [[y_get]]
// CHECK:                      OpStore %x [[y_get_int]]
    x = y.get();
}

// CHECK:         %Foo_set = OpFunction
// CHECK-NEXT: %param_this = OpFunctionParameter %_ptr_Function_Foo
// CHECK-NEXT:     %value_ = OpFunctionParameter %_ptr_Function_float

// CHECK:         %Foo_get = OpFunction %float
// CHECK-NEXT: [[this:%\w+]] = OpFunctionParameter %_ptr_Function_Foo

// CHECK: [[ptr_value:%\w+]] = OpAccessChain %_ptr_Function_float [[this]] %int_0
// CHECK:     [[value:%\w+]] = OpLoad %float [[ptr_value]]
// CHECK:                      OpReturnValue [[value]]
