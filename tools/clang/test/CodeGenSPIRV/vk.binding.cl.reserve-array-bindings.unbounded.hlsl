// RUN: %dxc -T ps_6_0 -E main -fspv-reserve-resource-array-bindings -fcgl  %s -spirv | FileCheck %s

// Regression test: an explicitly-bound unbounded (runtime-sized) resource
// array used to crash getNumBindingsUsedByResourceType, since its AST type
// (IncompleteArrayType) isn't a ConstantArrayType and isn't a resource type
// either, so the function fell through to its "unknown resource type"
// unreachable. An unbounded array must be the last resource declared in its
// register space, so nothing can collide with it regardless of how many
// slots are reserved for it -- it only reserves its own single binding
// number, same as without this flag. NextTexture (auto-assigned, no
// explicit register) is unaffected by MyTextures's position and lands on
// the first free slot, binding 0.
// CHECK: OpDecorate %MyTextures Binding 5
// CHECK: OpDecorate %NextTexture Binding 0
Texture2D    MyTextures[] : register(t5);
Texture2D    NextTexture;

float4 main(float2 TexCoord : TexCoord, uint index : Index) : SV_Target0
{
  return MyTextures[index].Load(int3(0, 0, 0)) + NextTexture.Load(int3(0, 0, 0));
}
