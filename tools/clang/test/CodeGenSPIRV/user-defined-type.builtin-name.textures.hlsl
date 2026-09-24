// RUN: %dxc -E main -T cs_6_5 -fspv-target-env=vulkan1.2 -spirv %s | FileCheck %s

// Verify that user-defined types in namespaces with the same names as HLSL
// built-in resource types are NOT misidentified as HLSL resource types. Each
// user-defined type should be lowered as a plain struct.

// CHECK-NOT: OpTypeSampledImage
// CHECK-NOT: OpTypeImage
// CHECK-NOT: OpTypeSampler
// CHECK-NOT: OpTypeAccelerationStructure

// CHECK: %float_300 = OpConstant %float 300
// CHECK: OpStore {{.*}} %float_300

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
  struct SubpassInput { float value; };
  struct SubpassInputMS { float value; };
}

RWStructuredBuffer<float> output : register(u0);

[numthreads(1,1,1)]
void main() {
  myns::Texture1D t1;
  t1.value = 1.0;
  myns::Texture2D t2;
  t2.r = 2.0; t2.g = 3.0;
  myns::Texture3D t3;
  t3.r = 4.0; t3.g = 5.0; t3.b = 6.0;
  myns::TextureCube tc;
  tc.x = 7.0;
  myns::Texture1DArray t1a;
  t1a.v = 8.0;
  myns::Texture2DArray t2a;
  t2a.v = 9.0;
  myns::Texture2DMS t2ms;
  t2ms.v = 10.0;
  myns::Texture2DMSArray t2msa;
  t2msa.v = 11.0;
  myns::TextureCubeArray tca;
  tca.v = 12.0;
  myns::RWTexture1D rwt1;
  rwt1.rw = 13.0;
  myns::RWTexture2D rwt2;
  rwt2.rw = 14.0;
  myns::RWTexture3D rwt3;
  rwt3.rw = 15.0;
  myns::RasterizerOrderedTexture2D rovt2;
  rovt2.rov = 16.0;
  myns::SamplerState ss;
  ss.id = 17;
  myns::SamplerComparisonState scs;
  scs.id = 18;
  myns::RaytracingAccelerationStructure rtas;
  rtas.x = 19.0;
  myns::Buffer buf;
  buf.data = 20.0;
  myns::RWBuffer rwbuf;
  rwbuf.data = 21.0;
  myns::RasterizerOrderedBuffer rovbuf;
  rovbuf.data = 22.0;
  myns::SubpassInput subpass;
  subpass.value = 23.0;
  myns::SubpassInputMS subpassms;
  subpassms.value = 24.0;
  output[0] = t1.value + t2.r + t2.g + t3.r + t3.g + t3.b + tc.x +
              t1a.v + t2a.v + t2ms.v + t2msa.v + tca.v + rwt1.rw +
              rwt2.rw + rwt3.rw + rovt2.rov + ss.id + scs.id + rtas.x +
              buf.data + rwbuf.data + rovbuf.data + subpass.value +
              subpassms.value;
}
