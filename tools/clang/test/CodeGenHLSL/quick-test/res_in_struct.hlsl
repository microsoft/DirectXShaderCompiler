// RUN: %dxc -T lib_6_2 %s | FileCheck %s

// TODO: make sure CreateHandleFromResourceStructForLib is called.
// CHECK: emit

struct M {
   float3 a;
   Texture2D<float4> tex;
};

float4 emit(M m)  {
   return m.tex.Load(m.a);
}