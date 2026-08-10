// RUN: %dxc -E main -T cs_6_5 -fspv-target-env=vulkan1.2 -spirv %s | FileCheck %s

// Verify that user-defined types in namespaces with the same names as HLSL
// built-in resource types are NOT misidentified as HLSL resource types. Each
// user-defined type should be lowered as a plain struct.

// CHECK-NOT: OpTypeSampledImage
// CHECK-NOT: OpTypeImage

// CHECK: %float_11 = OpConstant %float 11
// CHECK: OpStore {{.*}} %float_11

namespace myns {
  struct Texture1D { float value; };
  struct Texture2D { float r; float g; };
  struct Texture3D { float r; float g; float b; };
  struct TextureCube { float x; };
  struct Texture1DArray { float v; };
  struct Texture2DArray { float v; };
  struct Texture2DMS { float v; };
  struct Texture2DMSArray { float v; };
  struct TextureCubeArray { float v; };
  struct RWTexture1D { float rw; };
  struct RWTexture2D { float rw; };
  struct RWTexture3D { float rw; };
  struct RasterizerOrderedTexture2D { float rov; };
  struct SamplerState { int id; };
  struct SamplerComparisonState { int id; };
  struct RaytracingAccelerationStructure { float x; };
  struct Buffer { float data; };
  struct RWBuffer { float data; };
  struct RasterizerOrderedBuffer { float data; };
}

RWStructuredBuffer<float> output : register(u0);

[numthreads(1,1,1)]
void main() {
  myns::Texture1D t1;
  t1.value = 1.0;
  myns::Texture2D t2;
  t2.r = 2.0; t2.g = 3.0;
  myns::SamplerState ss;
  ss.id = 0;
  myns::Buffer buf;
  buf.data = 5.0;
  output[0] = t1.value + t2.r + t2.g + buf.data;
}
