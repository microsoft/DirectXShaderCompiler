// RUN: %dxc -T cs_6_0 -E main -HV 202x %s | FileCheck %s

// Verify HLSL 202x 'constexpr' enables constant folding through functions
// and constexpr variables.

constexpr int square(int x) { return x * x; }

constexpr int N = square(5); // 25

RWBuffer<int> buf;

[numthreads(1,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
  constexpr int local = square(3) + N; // 34
  buf[id.x] = local;
}

// CHECK: bufferStore
// The literal 34 should appear as the stored value.
// CHECK-SAME: i32 34, i32 34, i32 34, i32 34
