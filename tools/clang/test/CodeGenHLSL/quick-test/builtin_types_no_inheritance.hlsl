// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s

// CHECK: error: base 'vector' is marked 'final'
// CHECK: error: base 'matrix' is marked 'final'
// CHECK: error: base 'Texture3D' is marked 'final'
// CHECK: error: base 'ByteAddressBuffer' is marked 'final'
// CHECK: error: base 'SamplerState' is marked 'final'
// CHECK: error: base 'TriangleStream' is marked 'final'
// CHECK: error: base 'InputPatch' is marked 'final'
// CHECK: error: base 'OutputPatch' is marked 'final'
// CHECK: error: base 'RayDesc' is marked 'final'
// CHECK: error: base 'BuiltInTriangleIntersectionAttributes' is marked 'final'
// CHECK: error: base 'RaytracingAccelerationStructure' is marked 'final'
// CHECK: error: base 'GlobalRootSignature' is marked 'final'

struct F2 : float2 {};
struct F4x4 : float4x4 {};
struct Tex3D : Texture3D<float> {};
struct BABuf : ByteAddressBuffer {};
struct Samp : SamplerState {};

struct Vertex { float3 pos : POSITION; };
struct GSTS : TriangleStream<Vertex> {};
struct HSIP : InputPatch<Vertex, 16> {};
struct HSOP : OutputPatch<Vertex, 16> {};

struct RD : RayDesc {};
struct BITIA : BuiltInTriangleIntersectionAttributes {};
struct RTAS : RaytracingAccelerationStructure {};
struct GRS : GlobalRootSignature {};

float main() : SV_Target { return 0; }