// Run: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.2

// CHECK: OpEntryPoint Fragment %main "main"
// CHECK-SAME: %_Globals %gSampler %gTex %MyCBuffer %gCBuffer %gSPInput %gRWBuffer %out_var_SV_Target

int gScalar;
SamplerState gSampler;
float2 gVec;
Texture2D gTex;
float2x3 gMat1;
row_major float2x3 gMat2;
row_major float2x3 gArray[2];

struct S {
  float f;
};

S gStruct;

cbuffer MyCBuffer {
  float4 CBData[16];
};

ConstantBuffer<S> gCBuffer;

typedef SamplerState SamplerStateType;

struct {
  float2 f;
} gAnonStruct;

[[vk::input_attachment_index(0)]] SubpassInput gSPInput;

RWBuffer<float4> gRWBuffer[4];

float4 main() : SV_Target {
  return gScalar + gMat2[0][0] + gStruct.f;
}
