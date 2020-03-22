// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// CHECK: @dx.op.textureLoad.f32(i32 66,

Texture2D<float4> A, B;

struct ResStruct {
  Texture2D<float4> arr[2];
};

static ResStruct RS = { { A, B } };

float4 main() : OUT {
  return RS.arr[1].Load(uint3(0,0,0));
}

// TODO: The following still doesn't work because use of DxilValueCache
// does not extend to LoadAndStorePromoter used in PromoteStaticGlobalResources
float4 main2() : OUT {
  float4 result = 0;
  [unroll]
  for (uint i = 0; i < 2; i++)
    result += RS.arr[i].Load(uint3(0,0,0));
  return result;
}
