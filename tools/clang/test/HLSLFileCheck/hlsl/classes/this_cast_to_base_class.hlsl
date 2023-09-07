// RUN: %dxc -T lib_6_6 -HV 2021 -disable-lifetime-markers -fcgl %s | FileCheck %s

// `foo` calls `lib_func2` and copies out the temporary. Derived to base copies
// should probably always happen since HLSL doesn't have any real support for
// dynamic typing.

// CHECK-LABEL: define linkonce_odr void @"\01?foo@
// CHECK-SAME: (%class.Child* [[this:%.+]])
// Create a temporary.
// CHECK: [[OutArg:%.+]] = alloca %class.Parent
// CHECK: [[OutThisPtr:%.+]] = bitcast %class.Child* [[this]] to %class.Parent*

// Call lib_func2 with the temporary.
// CHECK: call void @"\01?lib_func2
// CHECK-SAME: (%class.Parent* dereferenceable(8) [[OutArg]])

// Copy the temporary back to the `this` object.
// CHECK: [[OutThisCpyPtr:%.+]] = bitcast %class.Parent* [[OutThisPtr]] to i8*
// CHECK: [[OutArgCpyPtr:%.+]] = bitcast %class.Parent* [[OutArg]] to i8*
// CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* [[OutThisCpyPtr]], i8* [[OutArgCpyPtr]], i64 8, i32 1, i1 false)

// `bar` calls `lib_func_3` with `this` as an `inout` parameter, so it needs to
// be initialized first, then copied back after the call.

// CHECK-LABEL: define linkonce_odr void @"\01?bar@
// CHEKC-SAME: (%class.Child* [[this:%.+]])
// CHECK: [[InOutArg:%.+]] = alloca %class.Parent

// Initialize the temporary from `this`.
// CHECK-DAG: [[ThisIPtr:%.+]] = getelementptr inbounds %class.Child, %class.Child* [[this]], i32 0, i32 0, i32 0
// CHECK-DAG: [[ThisFPtr:%.+]] = getelementptr inbounds %class.Child, %class.Child* [[this]], i32 0, i32 0, i32 1
// CHECK-DAG: [[ThisI:%.+]] = load i32, i32* [[ThisIPtr]]
// CHECK-DAG: [[ThisF:%.+]] = load float, float* [[ThisFPtr]]
// CHECK-DAG: [[TmpIPtr:%.+]] = getelementptr inbounds %class.Parent, %class.Parent* [[InOutArg]], i32 0, i32 0
// CHECK-DAG: [[TmpFPtr:%.+]] = getelementptr inbounds %class.Parent, %class.Parent* [[InOutArg]], i32 0, i32 1
// CHECK-DAG: store i32 [[ThisI]], i32* [[TmpIPtr]]
// CHECK-DAG: store float [[ThisF]], float* [[TmpFPtr]]

// Call lib_func3 with the temporary.
// CHECK-DAG: call void @"\01?lib_func3@{{[@$?.A-Za-z0-9_]+}}"(%class.Parent* dereferenceable(8) [[InOutArg]])

// Copy back the temporary to `this`. There is a redundant bitcast here due to
// the aggregate copy trying to match the target type before the memcpy is
// generated. This could be removed in the future.

// CHECK-DAG: [[ThisCastParent:%.+]] = bitcast %class.Child* [[this]] to %class.Parent*
// CHECK-DAG: [[ThisCastI8:%.+]] = bitcast %class.Parent* [[ThisCastParent]] to i8*
// CHECK-DAG: [[TmpCastI8:%.+]] = bitcast %class.Parent* [[InOutArg]] to i8*
// CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* [[ThisCastI8]], i8* [[TmpCastI8]], i64 8, i32 1, i1 false)


// CHECK-LABEL: define linkonce_odr i32 @"\01?foo@
// CHECK-SAME: (%class.Child* [[this:%.+]], i32 [[a:%.+]], i32 [[b:%.+]])
// CHECK: %[[Arg:.+]] = alloca %class.Parent

// Initialize the temporary from `this`.
// CHECK-DAG: [[ThisIPtr:%.+]] = getelementptr inbounds %class.Child, %class.Child* [[this]], i32 0, i32 0, i32 0
// CHECK-DAG: [[ThisFPtr:%.+]] = getelementptr inbounds %class.Child, %class.Child* [[this]], i32 0, i32 0, i32 1
// CHECK-DAG: [[ThisI:%.+]] = load i32, i32* [[ThisIPtr]]
// CHECK-DAG: [[ThisF:%.+]] = load float, float* [[ThisFPtr]]
// CHECK-DAG: [[TmpIPtr:%.+]] = getelementptr inbounds %class.Parent, %class.Parent* [[InOutArg]], i32 0, i32 0
// CHECK-DAG: [[TmpFPtr:%.+]] = getelementptr inbounds %class.Parent, %class.Parent* [[InOutArg]], i32 0, i32 1
// CHECK-DAG: store i32 [[ThisI]], i32* [[TmpIPtr]]
// CHECK-DAG: store float [[ThisF]], float* [[TmpFPtr]]

// Use the temporary to call lib_func.
// CHECK: call i32 @"\01?lib_func@
// CHECK-SAME: (%class.Parent* %[[Arg]], i32 %{{.*}}, i32 %{{.*}})

class Parent
{
    int i;
    float f;
};

int lib_func(Parent obj, int a, int b);
void lib_func2(out Parent obj);
void lib_func3(inout Parent obj);

class Child : Parent
{
    int foo(int a, int b)
    {
        return lib_func(this, a, b);
    }
    void foo() {
        lib_func2((Parent)this);
    }
    void bar() {
        lib_func3((Parent)this);
    }
    double d;
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
    c.foo();
    c.bar();
    gOut[0] = c.foo(111, 112);
}
