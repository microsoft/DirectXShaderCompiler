// RUN: %dxc -E main -T ps_6_0 -enable-templates %s | FileCheck %s

// CHECK:define void @main

template<typename T>
struct Test {

T t;
T foo(T t1) {
  return sin(t) * cos(t1);
}

};

float2 main(float4 a:A) : SV_Target {
  Test<float> t0;
  t0.t = a.x;
  Test<float2> t1;
  t1.t = a.xy;
  return t0.foo(a.y) + t1.foo(a.zw);
}