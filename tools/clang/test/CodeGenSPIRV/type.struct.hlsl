// Run: %dxc -T vs_6_0 -E main

// CHECK:      OpName %N "N"

// CHECK:      OpName %S "S"
// CHECK-NEXT: OpMemberName %S 0 "a"
// CHECK-NEXT: OpMemberName %S 1 "b"
// CHECK-NEXT: OpMemberName %S 2 "c"

// CHECK:      OpName %T "T"
// CHECK-NEXT: OpMemberName %T 0 "x"
// CHECK-NEXT: OpMemberName %T 1 "y"
// CHECK-NEXT: OpMemberName %T 2 "z"

// CHECK:      %N = OpTypeStruct
struct N {};

// CHECK:      %S = OpTypeStruct %uint %v4float %mat2v3float
struct S {
    uint a;
    float4 b;
    float2x3 c;
};

// CHECK:      %T = OpTypeStruct %S %v3int %S
struct T {
    S x;
    int3 y;
    S z;
};

void main() {
    N n;
    S s;
    T t;
}
