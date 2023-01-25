// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

struct A {
  float a;
  float b;
};

// Make sure noinline with struct argument and return type works.

// CHECK-DAG:define internal fastcc float @"\01?bar@@YAMUA@@@Z"(%struct.A* nocapture readonly %{{.+}})
// CHECK-DAG:define internal fastcc void @"\01?foo@@YA?AUA@@U1@@Z"(%struct.A* noalias nocapture sret %{{.+}}, %struct.A* nocapture readonly %{{.+}})
// CHECK:call fastcc void @"\01?foo@@YA?AUA@@U1@@Z"(%struct.A* nonnull sret %{{.+}}, %struct.A* nonnull %{{.+}})
// CHECK:call fastcc float @"\01?bar@@YAMUA@@@Z"(%struct.A* nonnull %{{.+}})
[noinline]
A  foo(A a) {
  return a;
}

[noinline]
float  bar(A a) {
  return a.a + a.b;
}

float main(A a:A) : SV_Target {
  return bar(foo(a));
}
