// RUN: dxc -Tps_6_0 %s -fcgl | FileCheck %s

// Make sure noinline argument is copy-in/copy-out.
// CHECK-LABEL: define float @main
// CHECK:%[[TMP:.+]] = alloca float
// CHECK-NEXT:  %call = call float @"\01?foo@@YAMAIAM@Z"(float* dereferenceable(4) %[[TMP]])
// CHECK-NEXT: %[[RESULT:.+]] = load float, float* %[[TMP]]
// CHECK-NEXT: store float %[[RESULT]], float* %depth, align 4
// CHECK-NEXT: ret float %call

float d;

[noinline]
float foo(out float depth) {
  depth = d;
  return d;
}

float main(out float depth: SV_Depth) : SV_Target {
  return foo(depth);
}
