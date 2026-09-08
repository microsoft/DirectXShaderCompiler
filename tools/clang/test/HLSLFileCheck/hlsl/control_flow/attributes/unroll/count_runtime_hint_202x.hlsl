// RUN: %dxc -E main -T ps_6_0 -HV 202x -O2 %s | FileCheck %s -check-prefixes=COMMON,O2
// RUN: %dxc -E main -T ps_6_0 -HV 202x -O3 %s | FileCheck %s -check-prefixes=COMMON,O3
// RUN: %dxc -E mainPowerOfTwo -T ps_6_0 -HV 202x -O2 %s | FileCheck %s -check-prefixes=COMMON,POWER-O2
// RUN: %dxc -E mainPowerOfTwo -T ps_6_0 -HV 202x -O3 %s | FileCheck %s -check-prefix=POWER-O3

// O2 Doesn't unroll and O3 Runtime unrolling only supports 
// power-of-two factors, so the count hint is
// consumed without cloning the loop body.
// COMMON: call float @dx.op.dot3
// COMMON-NOT: call float @dx.op.dot3
// COMMON: br i1 {{.*}}, label {{.*}}, label {{.*}}, !llvm.loop
// O2: !{!"llvm.loop.unroll.count", i32 3}
// O3: !{!"llvm.loop.unroll.disable"}
// POWER-O2: !{!"llvm.loop.unroll.count", i32 4}

// The O3 output has one remainder-loop body and four unrolled main-loop bodies.
// POWER-O3: and i32 {{.*}}, 3
// POWER-O3: call float @dx.op.dot3
// POWER-O3: call float @dx.op.dot3
// POWER-O3: call float @dx.op.dot3
// POWER-O3: call float @dx.op.dot3
// POWER-O3: call float @dx.op.dot3
// POWER-O3-NOT: call float @dx.op.dot3
// POWER-O3: add i32 {{.*}}, 4
// POWER-O3: br i1 {{.*}}, label {{.*}}, label {{.*}}, !llvm.loop
// POWER-O3: !{!"llvm.loop.unroll.disable"}

float main(float3 a : A, float3 b : B,
           uint iterationCount : COUNT) : SV_Target {
  float result = 0;
  [unroll(3)]
  for (uint i = 0; i < iterationCount; i++) {
    result += dot(a * i, b);
  }
  return result;
}

float mainPowerOfTwo(float3 a : A, float3 b : B,
                     uint iterationCount : COUNT) : SV_Target {
  float result = 0;
  [unroll(4)]
  for (uint i = 0; i < iterationCount; i++) {
    result += dot(a * i, b);
  }
  return result;
}
