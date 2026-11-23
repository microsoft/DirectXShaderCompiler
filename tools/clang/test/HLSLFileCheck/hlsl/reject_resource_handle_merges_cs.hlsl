// RUN: %dxc -T cs_6_6 -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_6 -E main -reject-resource-handle-merges %s | FileCheck %s -check-prefix=REJECT
// RUN: %dxc -T cs_6_6 -E main -Vd -reject-resource-handle-merges %s | FileCheck %s -check-prefix=REJECT
// RUN: %dxc -T lib_6_x -reject-resource-handle-merges %s | FileCheck %s

// After unrolling these loops, the optimizer leaves phis of the same buffer
// handle that feed annotateHandle. This passes validation, but some drivers
// fail to compile it. -reject-resource-handle-merges must turn it into a
// compile error, even without validation (-Vd). lib_6_x is not checked because
// it is only an input to offline linking; LinkerTest covers linking this
// shader to cs_6_6.
// If the optimizer stops creating these phis, the CHECK line fails and this
// shader needs to be replaced.

// CHECK: phi %dx.types.Handle

// REJECT: error: function 'main' merges resource handles with a phi/select, which is rejected because -reject-resource-handle-merges is set.
// REJECT-NEXT: note: at '%{{.*}} = phi %dx.types.Handle
// REJECT-NEXT: note: at '%{{.*}} = phi %dx.types.Handle

RWByteAddressBuffer buf : register(u0);

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  float acc = 0.0f;
  for (uint i = 0u; i < 4u; i++) {
    for (uint j = 0u; j < 4u; j++) {
      if (asfloat(buf.Load(((i * 4u + j) & 63u) * 4u)) > 1.0f) {
        acc += 1.0f;
        break;
      }
      for (uint k = 0u; k < 4u; k++) {
        if (asfloat(buf.Load(((i + j + k) & 63u) * 4u)) < 0.0f) {
          acc += 2.0f;
          break;
        }
        acc += asfloat(buf.Load(((i + j + k) & 63u) * 4u));
      }
      if (asfloat(buf.Load(((i + j) & 63u) * 4u)) > 5.0f)
        break;
    }
  }
  buf.Store(0u, asuint(acc));
}
