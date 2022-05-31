// RUN: %dxbc2dxil %s.dxbc /emit-llvm | %FileCheck %s -check-prefix=DXIL

// DXIL: !{i32 0, %dx.types.i8x28 addrspace(1)* undef, !"U0", i32 0, i32 1, i32 1, i32 12, i1 false, i1 true

struct Foo {
  float4 a;
  float3 b;
};


RWStructuredBuffer<Foo> buf2;

int main() : SV_Target
{
  return buf2.IncrementCounter();
}