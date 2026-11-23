// RUN: %dxc -E main -T cs_6_5 -fspv-target-env=vulkan1.2 -spirv %s | FileCheck %s

// Verify that user-defined types in namespaces with the same names as HLSL
// built-in buffer types are NOT misidentified as HLSL resource types. The
// user-defined types should compile correctly without errors.

// CHECK: OpCapability Shader
// CHECK: OpStore

namespace myns {
  struct StructuredBuffer { float data; };
  struct RWStructuredBuffer { float data; };
  struct AppendStructuredBuffer { float data; };
  struct ConsumeStructuredBuffer { float data; };
  struct ByteAddressBuffer { int raw; };
  struct RWByteAddressBuffer { int raw; };
  struct ConstantBuffer { float v; };
  struct TextureBuffer { float v; };
}

RWStructuredBuffer<float> output : register(u0);

[numthreads(1,1,1)]
void main() {
  myns::StructuredBuffer sb;
  sb.data = 1.0;
  myns::RWStructuredBuffer rwsb;
  rwsb.data = 2.0;
  myns::AppendStructuredBuffer asb;
  asb.data = 3.0;
  myns::ConsumeStructuredBuffer csb;
  csb.data = 4.0;
  myns::ByteAddressBuffer bab;
  bab.raw = 42;
  myns::RWByteAddressBuffer rwbab;
  rwbab.raw = 43;
  myns::ConstantBuffer cb;
  cb.v = 5.0;
  myns::TextureBuffer tb;
  tb.v = 6.0;
  output[0] = sb.data + rwsb.data + asb.data + csb.data + bab.raw +
              rwbab.raw + cb.v + tb.v;
}
