// RUN: %dxc -E main -T ps_6_0 %s

// TODO: create execution test.
// CHECK: [16 x float] [float 1.500000e+01, float 1.600000e+01, float 1.700000e+01, float 1.800000e+01, float 1.500000e+01, float 1.600000e+01, float 1.700000e+01, float 1.800000e+01, float 1.500000e+01, float 1.600000e+01, float 1.700000e+01, float 1.800000e+01, float 1.500000e+01, float 1.600000e+01, float 1.700000e+01, float 1.800000e+01]

static float4 f0 = {5,6,7,8};
static float4 f1 = 0;
static float4 f2 = {0,0,0,0};
static float4 f3[] = { f0, f1, f2 };
static float4x4 worldMatrix = { {0,0,0,0}, {1,1,1,1}, {2,2,2,2}, {3,3,3,3} };

static float4x4 m0 = { 15,16,17,18,
                       15,16,17,18,
                       15,16,17,18,
                       15,16,17,18 };

static float2x2 m2[4] = { 25,26,27,28,
                       25,26,27,28,
                       25,26,27,28,
                       25,26,27,28 };


uint i;

float4 main() : SV_TARGET {
  m2[i][1][i] = m0[i][i];
  return f2 + f1 + f0[i] + m2[1]._m00_m01_m00_m10 + m0[i] + f3[0] + worldMatrix[1];
}
