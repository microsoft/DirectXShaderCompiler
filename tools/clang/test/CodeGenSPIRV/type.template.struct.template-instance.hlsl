// Run: %dxc -T ps_6_0 -E main -enable-templates

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
// CHECK: %y = OpVariable %_ptr_Function_Foo Function

// CHECK:         %Foo_set = OpFunction
// CHECK-NEXT: %param_this = OpFunctionParameter %_ptr_Function_Foo
// CHECK-NEXT:     %value_ = OpFunctionParameter %_ptr_Function_float

// CHECK:         %Foo_get = OpFunction %float None %36
// CHECK-NEXT: [[this:%\w+]] = OpFunctionParameter %_ptr_Function_Foo

// CHECK: [[ptr_value:%\w+]] = OpAccessChain %_ptr_Function_float [[this]] %int_0
// CHECK:     [[value:%\w+]] = OpLoad %float [[ptr_value]]
// CHECK:                      OpReturnValue [[value]]

    int x;
    Foo<float> y;

    y.set(x);
    x = y.get();
}
