// RUN: %dxc /Tps_6_0 /Emain > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry

#define MAX_INDEX 5

groupshared float g_Array[2][(MAX_INDEX * MAX_INDEX)];

[RootSignature("")] float4 main(uint GroupIndex
                                : A) : SV_Target {
  uint idx;
  float l_Array[(MAX_INDEX * MAX_INDEX)] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  for (idx = 0; idx < (MAX_INDEX * MAX_INDEX); idx++) {
    g_Array[GroupIndex][idx] = l_Array[idx];
  }

  return float4(g_Array[GroupIndex][0], g_Array[GroupIndex][1], g_Array[GroupIndex][2], g_Array[GroupIndex][3]);
}