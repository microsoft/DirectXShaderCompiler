// RUN: %dxc -Zi -E main -Od -T ps_6_0 %s -DFORCE_UNROLL | FileCheck %s
// RUN: %dxc -Zi -E main -T ps_6_0 %s -DFORCE_UNROLL | FileCheck %s

// CHECK: %{{.+}} = call float @dx.op.unary.f32(i32 13
// CHECK: %{{.+}} = call float @dx.op.unary.f32(i32 13
// CHECK: %{{.+}} = call float @dx.op.unary.f32(i32 13
// CHECK: %{{.+}} = call float @dx.op.unary.f32(i32 13
// CHECK: %{{.+}} = call float @dx.op.unary.f32(i32 13

// Make sure we didn't transform
// CHECK-NOT: dx.struct_exit

#ifdef FORCE_UNROLL
#define UNROLL [unroll]
#else
#define UNROLL
#endif
#define COUNT 5

Texture2D tex0;
RWTexture1D<float> uav0;
RWTexture1D<float> uav1;

const uint idx;

[RootSignature("CBV(b0), DescriptorTable(SRV(t0)), DescriptorTable(UAV(u0), UAV(u1))")]
float main(uint a : A, uint b : B, uint c : C) : SV_Target {

  float ret = 0;
  float array[COUNT] = {1.0, 2.0, 3.0, 4.0, 5.0};

  UNROLL for(uint i = 1; i <= COUNT; i++) {

    if ((a * i) & c) {
      ret += sin(i * b); // check for sin

      if ((a * i) & b) {
        if ((c | a) & b) {
          // loop exit here
          uav0[i] += a;
          return 1;
        }

        // This loop should prevent the struct_exit transformation
        // from happening.
        [loop] for (uint j = 0; j < 5; j++) {
          uav1[j] += j*c;
        }

        array[(idx + i) % 5] += a;
      }
    }
  }

  return ret + array[0];
}

// Exclude quoted source file (see readme)
// CHECK-LABEL: {{!"[^"]*\\0A[^"]*"}}

