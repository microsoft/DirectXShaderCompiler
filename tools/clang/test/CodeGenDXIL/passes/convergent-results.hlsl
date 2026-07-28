// REQUIRES: dxil-1-9
// XFAIL: *
// RUN: %dxc -T cs_6_6 -DSCALAR -DFUNC=ddx %s        | FileCheck %s
// RUN: %dxc -T cs_6_6 -DSCALAR -DFUNC=ddx_coarse %s | FileCheck %s
// RUN: %dxc -T cs_6_6 -DSCALAR -DFUNC=ddx_fine %s   | FileCheck %s
// RUN: %dxc -T cs_6_6 -DSCALAR -DFUNC=ddy %s        | FileCheck %s
// RUN: %dxc -T cs_6_6 -DSCALAR -DFUNC=ddy_coarse %s | FileCheck %s
// RUN: %dxc -T cs_6_6 -DSCALAR -DFUNC=ddy_fine %s   | FileCheck %s
// RUN: %dxc -T cs_6_9 -DFUNC=ddx %s                  | FileCheck %s
// RUN: %dxc -T cs_6_9 -DFUNC=ddx_coarse %s           | FileCheck %s
// RUN: %dxc -T cs_6_9 -DFUNC=ddx_fine %s             | FileCheck %s
// RUN: %dxc -T cs_6_9 -DFUNC=ddy %s                  | FileCheck %s
// RUN: %dxc -T cs_6_9 -DFUNC=ddy_coarse %s           | FileCheck %s
// RUN: %dxc -T cs_6_9 -DFUNC=ddy_fine %s             | FileCheck %s

RWByteAddressBuffer output;

[numthreads(2, 2, 1)]
void main() {
  uint laneIndex = WaveGetLaneIndex();

#ifdef SCALAR
  float value = float(laneIndex * 2);
  float result = FUNC(value);
#else
  vector<float, 3> value = 1.0;
  value += float(laneIndex * 2);
  vector<float, 3> result = FUNC(value);
#endif

  // Derivatives require all quad lanes, so the call must remain before the
  // divergent branch even though only lane 3 consumes the result.
  // CHECK: call {{(<3 x float>|float)}} @dx.op.unary
  // CHECK: icmp eq i32
  // CHECK-NEXT: br i1
  if (laneIndex == 3) {
#ifdef SCALAR
    output.Store(0, asuint(result));
#else
    output.Store<vector<float, 3> >(0, result);
#endif
  }
}
