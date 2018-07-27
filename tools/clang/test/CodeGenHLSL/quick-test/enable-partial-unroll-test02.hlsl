// RUN: %dxc /Tps_6_0 /Emain > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry

#define MAX_INDEX 14

groupshared float g_Array[2][(MAX_INDEX * MAX_INDEX)];

[RootSignature("")] float4 main(uint GroupIndex
                                : A) : SV_Target {
  uint idx;
  for (idx = 0; idx < (MAX_INDEX * MAX_INDEX); idx++) {
    g_Array[GroupIndex][idx] = 0.0f;
  }

  return float4(g_Array[GroupIndex][0], g_Array[GroupIndex][1], g_Array[GroupIndex][2], g_Array[GroupIndex][3]);
}