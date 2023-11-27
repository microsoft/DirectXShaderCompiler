// RUN: %dxc -T ps_6_2 -E PSMain

// The value for the enum is stored in a variable, which will be optimized
// away when optimizations are enabled.
// CHECK: [[enum_var:%\w+]] = OpVariable %_ptr_Private_int Private %int_1
struct TestStruct {
    enum EnumInTestStruct {
        A = 1,
    };
};

// CHECK: %testFunc = OpFunction %int
// CHECK-NEXT: OpLabel
// CHECK-NEXT: [[ld:%\w+]] = OpLoad %int [[enum_var]]
// CHECK-NEXT: OpReturnValue [[ld]]
TestStruct::EnumInTestStruct testFunc() {
    return TestStruct::A;
}

uint PSMain() : SV_TARGET
{
    TestStruct::EnumInTestStruct i = testFunc();
    return i;
}
