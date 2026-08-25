// RUN: %dxc -E main -T ps_6_0 -HV 202x -O2 %s | FileCheck %s -check-prefix=O2
// RUN: %dxc -E main -T ps_6_0 -HV 202x -O3 %s | FileCheck %s -check-prefix=O3

// O2: call float @dx.op.dot3
// O2-NOT: call float @dx.op.dot3
// O2: br i1 {{.*}}, label {{.*}}, label {{.*}}, !llvm.loop
// O2: !{!"llvm.loop.unroll.count", i32 3}

// O3: call float @dx.op.dot3
// O3: call float @dx.op.dot3
// O3: call float @dx.op.dot3
// O3: call float @dx.op.dot3
// O3-NOT: call float @dx.op.dot3
// O3: br i1 {{.*}}, label {{.*}}, label {{.*}}, !llvm.loop
// O3: !{!"llvm.loop.unroll.disable"}

float main(float3 a : A, float3 b : B) : SV_Target {
  float result = 0;
  [unroll(3)]
  for (int i = 0; i < 10; i++) {
    result += dot(a * i, b);
  }
  return result;
}
