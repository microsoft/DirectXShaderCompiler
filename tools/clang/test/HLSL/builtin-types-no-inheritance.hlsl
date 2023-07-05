// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify -verify-ignore-unexpected=note %s

struct F2 : float2 {}; // expected-error {{base 'vector' is marked 'final'}} fxc-error {{X3094: base type is not a struct, class or interface}}
struct F4x4 : float4x4 {}; // expected-error {{base specifier must name a class}} fxc-error {{X3094: base type is not a struct, class or interface}}
struct Tex3D : Texture3D<float> {};  // expected-error {{base 'Texture3D' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'Texture3D'}}
struct BABuf : ByteAddressBuffer {}; // expected-error {{base 'ByteAddressBuffer' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'ByteAddressBuffer'}}
struct StructBuf : StructuredBuffer<int> {}; // expected-error {{base 'StructuredBuffer' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'StructuredBuffer'}}
struct Samp : SamplerState {}; // expected-error {{base 'SamplerState' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'SamplerState'}}

struct Vertex { float3 pos : POSITION; };
struct GSTS : TriangleStream<Vertex> {}; // expected-error {{base 'TriangleStream' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'TriangleStream'}}
struct HSIP : InputPatch<Vertex, 16> {}; // expected-error {{base 'InputPatch' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'InputPatch'}}
struct HSOP : OutputPatch<Vertex, 16> {}; // expected-error {{base 'OutputPatch' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'OutputPatch'}}

struct RD : RayDesc {}; // expected-error {{base 'RayDesc' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'RayDesc'}}
struct BITIA : BuiltInTriangleIntersectionAttributes {}; // expected-error {{base 'BuiltInTriangleIntersectionAttributes' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'BuiltInTriangleIntersectionAttributes'}}
struct RTAS : RaytracingAccelerationStructure {}; // expected-error {{base 'RaytracingAccelerationStructure' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'RaytracingAccelerationStructure'}}
struct GRS : GlobalRootSignature {}; // expected-error {{base 'GlobalRootSignature' is marked 'final'}} fxc-error {{X3000: syntax error: unexpected token 'GlobalRootSignature'}}

void main() {}