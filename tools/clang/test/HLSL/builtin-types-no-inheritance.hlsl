// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify -verify-ignore-unexpected=note %s

struct F2 : float2 {}; // expected-error {{base 'vector' is marked 'final'}}
struct F4x4 : float4x4 {}; // expected-error {{base 'matrix' is marked 'final'}}
struct Tex3D : Texture3D<float> {};  // expected-error {{base 'Texture3D' is marked 'final'}}
struct BABuf : ByteAddressBuffer {}; // expected-error {{base 'ByteAddressBuffer' is marked 'final'}}
struct StructBuf : StructuredBuffer<int> {}; // expected-error {{base 'StructuredBuffer' is marked 'final'}}
struct Samp : SamplerState {}; // expected-error {{base 'SamplerState' is marked 'final'}}

struct Vertex { float3 pos : POSITION; };
struct GSTS : TriangleStream<Vertex> {}; // expected-error {{base 'TriangleStream' is marked 'final'}}
struct HSIP : InputPatch<Vertex, 16> {}; // expected-error {{base 'InputPatch' is marked 'final'}}
struct HSOP : OutputPatch<Vertex, 16> {}; // expected-error {{base 'OutputPatch' is marked 'final'}}

struct RD : RayDesc {}; // expected-error {{base 'RayDesc' is marked 'final'}}
struct BITIA : BuiltInTriangleIntersectionAttributes {}; // expected-error {{base 'BuiltInTriangleIntersectionAttributes' is marked 'final'}}
struct RTAS : RaytracingAccelerationStructure {}; // expected-error {{base 'RaytracingAccelerationStructure' is marked 'final'}}
struct GRS : GlobalRootSignature {}; // expected-error {{base 'GlobalRootSignature' is marked 'final'}}

void main() {}