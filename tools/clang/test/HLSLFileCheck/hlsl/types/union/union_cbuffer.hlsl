// RUN: %dxc -E main -T ps_6_0 -enable-unions -HV 202x %s | FileCheck %s

// CHECK:define void @main
union U {
  float4 v;
  uint x;
};

cbuffer Foo {
  U g;
};

float4 main(int2 a
            : A) : SV_TARGET {
  // CHECK:extractvalue %dx.types.CBufRet.f32 {{.*}}, 0
  return g.v;
}
