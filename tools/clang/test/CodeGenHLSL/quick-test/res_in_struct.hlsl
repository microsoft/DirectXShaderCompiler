// RUN: %dxc -T lib_6_2 %s | FileCheck %s

// make sure CreateHandleForLib is called.
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.Texture2D<vector<float, 4> >"(i32 160,

struct M {
   float3 a;
   Texture2D<float4> tex;
};

float4 emit(M m)  {
   return m.tex.Load(m.a);
}