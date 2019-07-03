// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s

// Make sure (Base)v2p.b = (Base)d; create storeOutput.
// CHECK:call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float
// CHECK:call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float
// CHECK:call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float
// CHECK:call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float


struct Base
{
    float4 c : C;
};

struct Derived : Base
{
};


struct VStoPS
{
    float4 a : A;
    Derived b;
    float4 p : SV_Position;
};


VStoPS main(float4 a : A, float4 b : B, float4 p : P)
{
    Derived d = (Derived)b;

    VStoPS v2p;
    v2p.a = a;
    (Base)v2p.b = (Base)d;
    v2p.p = p;
    return v2p;
}