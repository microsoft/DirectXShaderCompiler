// RUN: %dxreflector %s | FileCheck %s

typedef RWByteAddressBuffer RWBAB;

RWByteAddressBuffer output0;
RWByteAddressBuffer output1 	    : SEMA;
RWByteAddressBuffer output2 	    : register(u1);
RWByteAddressBuffer output3[12];
RWBAB output4;

struct Test {
    float a;
};

typedef float f32;

typedef StructuredBuffer<float> sbufferFloat;
typedef StructuredBuffer<f32> sbufferF32;
typedef StructuredBuffer<Test> sbufferTest;

StructuredBuffer<float> input0;
StructuredBuffer<f32> input1;
StructuredBuffer<Test> input2;
sbufferFloat input3;
sbufferF32 input4;
sbufferTest input5;

typedef ByteAddressBuffer BAB;

ByteAddressBuffer input6;
BAB input7;

typedef Texture2D t2d;
typedef Texture3D t3d;
typedef TextureCube tcub;
typedef Texture1D t1d;
typedef Texture2DMS<float4> t2dms;

Texture2D input8;
Texture3D input9;
TextureCube input10;
Texture1D input11;
Texture2DMS<float4> input12;

t2d input13;
t3d input14;
tcub input15;
t1d input16;
t2dms input17;

typedef Texture2D<uint4> t2duint;

Texture2D<uint4> input18;
t2duint input19;

typedef SamplerState samp;

SamplerState sampler0;
samp sampler1;

cbuffer test : register(b0) { float a; };
cbuffer test2 { float b; };

typedef ConstantBuffer<Test> cbufTest;

ConstantBuffer<Test> test3;
cbufTest test4;

float e;

//TODO: Disable listing this as if it's a struct, but needs a builtin type in DX headers.
typedef RaytracingAccelerationStructure rtas;

RaytracingAccelerationStructure rtas0;
rtas rtas1;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint id : SV_DispatchThreadID) { }

// CHECK: {
// CHECK:         "Features": [
// CHECK:                 "Basics",
// CHECK:                 "Functions",
// CHECK:                 "Namespaces",
// CHECK:                 "UserTypes",
// CHECK:                 "Scopes",
// CHECK:                 "Symbols"
// CHECK:         ],
// CHECK:         "Children": [
// CHECK:                 {
// CHECK:                         "Name": "RWBAB",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "RWByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "output0",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "RWByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "output1",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Semantic": "SEMA",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "RWByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "output2",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "RWByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "output3",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "RWByteAddressBuffer",
// CHECK:                                 "BindCount": 12
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "output4",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "RWByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "sbufferFloat",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "StructuredBuffer",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "float"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "sbufferF32",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "StructuredBuffer",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "f32",
// CHECK:                                                 "UnderlyingName": "float"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "sbufferTest",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "StructuredBuffer",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "Test"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input0",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input1",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32",
// CHECK:                                                 "UnderlyingName": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input2",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "a",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input3",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input4",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32",
// CHECK:                                                 "UnderlyingName": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input5",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "a",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "BAB",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "ByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input6",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "ByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input7",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "ByteAddressBuffer"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "t2d",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Texture2D",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "vector<float, 4>",
// CHECK:                                                 "UnderlyingName": "float4"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "t3d",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Texture3D",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "vector<float, 4>",
// CHECK:                                                 "UnderlyingName": "float4"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "tcub",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "TextureCube",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "vector<float, 4>",
// CHECK:                                                 "UnderlyingName": "float4"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "t1d",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Texture1D",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "vector<float, 4>",
// CHECK:                                                 "UnderlyingName": "float4"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "t2dms",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Texture2DMS",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "float4"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input8",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input9",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture3D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input10",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "TextureCube",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input11",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture1D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input12",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2DMS",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input13",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input14",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture3D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input15",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "TextureCube",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input16",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture1D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input17",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2DMS",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "t2duint",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Texture2D",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "$Element",
// CHECK:                                                 "TypeName": "uint4"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input18",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2D",
// CHECK:                                 "ReturnType": "uint",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "input19",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2D",
// CHECK:                                 "ReturnType": "uint",
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "samp",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "SamplerState"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "sampler0",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "SamplerState"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "sampler1",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "SamplerState"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "cbuffer",
// CHECK:                                 "Flags": [
// CHECK:                                         "UserPacked"
// CHECK:                                 ]
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test2",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "cbuffer",
// CHECK:                                 "Flags": [
// CHECK:                                         "UserPacked"
// CHECK:                                 ]
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "b",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "cbufTest",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "ConstantBuffer",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "Test",
// CHECK:                                                 "TypeName": "Test"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test3",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "cbuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "test3",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "a",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test4",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "cbuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "test4",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "a",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "e",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                        "Name": "rtas",
// CHECK:                        "NodeType": "Typedef",
// CHECK:                        "Type": {
// CHECK:                                "Name": "RaytracingAccelerationStructure",
// CHECK:                                "Members": [
// CHECK:                                        {
// CHECK:                                                "Name": "h",
// CHECK:                                                "TypeName": "int"
// CHECK:                                        }
// CHECK:                                ]
// CHECK:                        }
// CHECK:                },
// CHECK:                {
// CHECK:                        "Name": "rtas0",
// CHECK:                        "NodeType": "Register",
// CHECK:                        "Register": {
// CHECK:                                "RegisterType": "RaytracingAccelerationStructure"
// CHECK:                        }
// CHECK:                },
// CHECK:                {
// CHECK:                        "Name": "rtas1",
// CHECK:                        "NodeType": "Register",
// CHECK:                        "Register": {
// CHECK:                                "RegisterType": "RaytracingAccelerationStructure"
// CHECK:                        }
// CHECK:                }
// CHECK:                 {
// CHECK:                         "Name": "main",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Annotations": [
// CHECK:                                 "[shader(\"compute\")]"
// CHECK:                         ],
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "id": {
// CHECK:                                                 "TypeName": "uint",
// CHECK:                                                 "Semantic": "SV_DispatchThreadID"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }