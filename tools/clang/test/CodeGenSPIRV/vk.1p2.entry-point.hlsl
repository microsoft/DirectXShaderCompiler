// Run: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.2

// CHECK: OpEntryPoint Fragment %main "main"
// CHECK-SAME: %_Globals %gSampler %gTex %MyCBuffer %gCBuffer %gSPInput %gRWBuffer %out_var_SV_Target

          int           gScalar;   // 0
          SamplerState  gSampler;  // Not included - 1
          float2        gVec;      // 1
          Texture2D     gTex;      // Not included - 2
          float2x3      gMat1;     // 2
row_major float2x3      gMat2;     // 3

row_major float2x3      gArray[2]; // 4

struct S {
    float f;
};

          S             gStruct;   // 5

cbuffer MyCBuffer {                // Not included - 4
    float4 CBData[16];
};

ConstantBuffer<S>       gCBuffer;  // Not included - 5

typedef SamplerState SamplerStateType; // Not included - type definition

struct {
    float2 f;
}                       gAnonStruct; // 6

[[vk::input_attachment_index(0)]]
SubpassInput            gSPInput;  // Not included - 8

RWBuffer<float4>        gRWBuffer[4]; // Not included - 9 (array)

float4 main() : SV_Target {
    return gScalar + gMat2[0][0] + gStruct.f;
}
