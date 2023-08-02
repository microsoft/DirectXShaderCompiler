// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

struct MyData {
   float3 x[4];  
};

StructuredBuffer<MyData> DataIn;
RWStructuredBuffer<MyData> DataOut;

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchid : SV_DispatchThreadID)
{
    const MyData data = DataIn[dispatchid.x];
    data.x[0] = 1.0f; // expected-error {{read-only variable is not assignable}} fxc-error {{X3025: l-value specifies const object}}
    DataOut[dispatchid.x] = data;
}
