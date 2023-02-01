// RUN: %dxc -T lib_6_6 -HV 2021 -fcgl %s | FileCheck %s

// CHECK-LABLE: define linkonce_odr i32 @"\01?foo@Child@@QAAHHH@Z"(%class.Child* %this, i32 %a, i32 %b)
// CHECK: %[[Arg:.+]] = alloca %class.Parent
// CHECK: %[[Tmp:.+]] = alloca %class.Parent, align 4

// Make sure this is copy to agg tmp.
// CHECK: %[[PtrI:.+]] = getelementptr inbounds %class.Child, %class.Child* %this, i32 0, i32 0, i32 0
// CHECK: %[[PtrJ:.+]] = getelementptr inbounds %class.Child, %class.Child* %this, i32 0, i32 0, i32 1
// CHECK: %[[I:.+]] = load i32, i32* %[[PtrI]]
// CHECK: %[[J:.+]] = load float, float* %[[PtrJ]]
// CHECK: %[[TmpPtrI:.+]] = getelementptr inbounds %class.Parent, %class.Parent* %[[Tmp]], i32 0, i32 0
// CHECK: %[[TmpPtrJ:.+]] = getelementptr inbounds %class.Parent, %class.Parent* %[[Tmp]], i32 0, i32 1
// CHECK: store i32 %[[I]], i32* %[[TmpPtrI]]
// CHECK: store float %[[J]], float* %[[TmpPtrJ]]

// Skip lifetime marker
// CHECK: %[[ArgPtrForMarker:.+]] = bitcast %class.Parent* %[[Arg]] to i8*
// CHECK: call void @llvm.lifetime.start(i64 8, i8* %[[ArgPtrForMarker]])

// Make sure Tmp copy to Arg.
// CHECK: %[[ArgPtr:.+]] = bitcast %class.Parent* %[[Arg]] to i8*
// CHECK: %[[TmpPtr:.+]] = bitcast %class.Parent* %[[Tmp]] to i8*
// CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* %[[ArgPtr]], i8* %[[TmpPtr]], i64 8, i32 1, i1 false)

// Use Arg to call lib_func.
// CHECK: call i32 @"\01?lib_func@@YAHVParent@@HH@Z"(%class.Parent* %[[Arg]], i32 %11, i32 %10)

class Parent
{
    int i;
    float f;
};

int lib_func(Parent obj, int a, int b);

class Child : Parent
{
    int foo(int a, int b)
    {
        return lib_func(this, a, b);
    }
};

#define RS \
    "RootFlags(0), " \
    "DescriptorTable(UAV(u0))"

RWBuffer<uint> gOut : register(u0);

[shader("compute")]
[RootSignature(RS)]
[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Child c;
    c.i = 110;
    gOut[0] = c.foo(111, 112);
}
