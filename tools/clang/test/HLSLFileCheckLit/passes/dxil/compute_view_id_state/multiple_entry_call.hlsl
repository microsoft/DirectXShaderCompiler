// RUN: %dxc -Tps_6_0 %s | FileCheck %s

// Just make sure it not crash.
// CHECK: define void @main()
// CHECK: call fastcc float @"\01?foo@@YAMM@Z"(float %{{.+}})
// CHECK: call fastcc float @"\01?foo@@YAMM@Z"(float %{{.+}})

[noinline]
float foo(float a) {
   return a+2;
}

float main(float2 a : A) : SV_Target {
  return foo(a.x) + foo(a.y);
}
