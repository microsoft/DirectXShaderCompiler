// Run: %dxc -T vs_6_0 -E main

struct S {
    static const int FIVE = 5;
    static const int SIX;
};

const int S::SIX = 6;

class T {
    static const int FIVE = 5;
    static const int SIX;
};

const int T::SIX = 6;

int foo(int val) { return val; }

// CHECK:   %FIVE = OpVariable %_ptr_Private_int Private %int_5
// CHECK:    %SIX = OpVariable %_ptr_Private_int Private %int_6
// CHECK: %FIVE_0 = OpVariable %_ptr_Private_int Private %int_5
// CHECK:  %SIX_0 = OpVariable %_ptr_Private_int Private %int_6
int main() : A {
// CHECK: OpLoad %int %FIVE
// CHECK: OpLoad %int %SIX
// CHECK: OpLoad %int %FIVE_0
// CHECK: OpLoad %int %SIX_0
    return foo(S::FIVE) + foo(S::SIX) + foo(T::FIVE) + foo(T::SIX);
}
