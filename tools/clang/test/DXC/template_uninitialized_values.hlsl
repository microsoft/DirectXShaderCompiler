// RUN: dxc -E main -T cs_6_6 -HV 2021 -Wno-unused-value %s | FileCheck %s

// Asserts in UninitializedValues.cpp were triggered by the following code
// examples. The cause was that HLSL out parameters or local variables from
// template instantiations may not be present in the declToIndex map when the
// variable's DeclContext differs from the analysis context (common in
// template instantiations).
// The fix was to add defensive checks so that if the variable isn't tracked
// it is silently ignored.

// CHECK: define void @main()

template <typename R>
void test(R x, out uint result) {
    uint repro = 0;
    result = 10;
}

[numthreads(32, 32, 1)] void main(uint2 threadId: SV_DispatchThreadID) {
    uint x;
    test(10, x);
}

template <typename NameType>
void func2(out uint var1)
{
    uint var3;
    uint var4;
    uint var5;
    uint var6;
    uint var7;
    uint var8;
    uint var9;
    uint var10;
    uint var11;
    uint var12;
    uint var13;
    uint var14;
    uint var15;
    uint var16;
    uint var17;
    uint var18;
    uint var19;
    uint var20;
    uint var21;
    uint var22;
    uint var23;
    uint var24;
    uint var25;
    uint var26;
    uint var27;
    uint var28;
    uint var29;
    uint var30;
    uint var31;
    var1;
}

// Exercise the defensive ClassifyRefs::Init path when an unmapped out
// parameter is forwarded to another out parameter through a template call.
template <typename NameType>
void func3(out uint var1, out uint var2)
{
    var2 = var1;
}

template <typename NameType>
void func4(out uint var1)
{
    uint var2;
    func3<NameType>(var1, var2);
}

void func1()
{
    uint var33;
    func2<uint>(var33);

    uint var34;
    func4<uint>(var34);
}
