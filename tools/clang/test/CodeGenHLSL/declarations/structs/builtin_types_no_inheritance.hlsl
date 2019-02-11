// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

// CHECK: error: base 'vector' is marked 'final'
struct F2 : float2 {};
// CHECK: error: base 'matrix' is marked 'final'
struct F4x4 : float4x4 {};
// CHECK: error: base 'Texture3D' is marked 'final'
struct Tex3D : Texture3D<float> {};
// CHECK: error: base 'ByteAddressBuffer' is marked 'final'
struct BABuf : ByteAddressBuffer {};
// CHECK: error: base 'StructuredBuffer' is marked 'final'
struct StructBuf : StructuredBuffer<int> {};
// CHECK: error: base 'SamplerState' is marked 'final'
struct Samp : SamplerState {};

struct Vertex { float3 pos : POSITION; };
// CHECK: error: base 'TriangleStream' is marked 'final'
struct GSTS : TriangleStream<Vertex> {};
// CHECK: error: base 'InputPatch' is marked 'final'
struct HSIP : InputPatch<Vertex, 16> {};
// CHECK: error: base 'OutputPatch' is marked 'final'
struct HSOP : OutputPatch<Vertex, 16> {};

// CHECK: error: base 'RayDesc' is marked 'final'
struct RD : RayDesc {};
// CHECK: error: base 'BuiltInTriangleIntersectionAttributes' is marked 'final'
struct BITIA : BuiltInTriangleIntersectionAttributes {};
// CHECK: error: base 'RaytracingAccelerationStructure' is marked 'final'
struct RTAS : RaytracingAccelerationStructure {};
// CHECK: error: base 'GlobalRootSignature' is marked 'final'
struct GRS : GlobalRootSignature {};

void main() {}