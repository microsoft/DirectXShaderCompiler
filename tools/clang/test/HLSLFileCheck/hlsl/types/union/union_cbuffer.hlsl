// RUN: %dxc -E main -T ps_6_0 -HV 202x %s | FileCheck %s

// CHECK: %Foo = type { %union.U }
// CHECK: %union.U = type { <4 x float> }

// CHECK:define void @main
union U {
  uint x;
  float4 v;
};

cbuffer Foo {
  U g;
};

float4 main(int2 a
            : A) : SV_TARGET {
  // CHECK:extractvalue %dx.types.CBufRet.f32 {{.*}}, 0
  return g.v;
}
