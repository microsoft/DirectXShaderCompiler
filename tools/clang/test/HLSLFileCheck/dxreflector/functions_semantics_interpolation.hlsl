// RUN: %dxreflector %s | FileCheck %s

struct Test {
    float3 pos : POSITION0;
    nointerpolation uint test : FLAT0;
};

void testFunction(inout uint a, out uint b, in uint c) {
	b = a | c;
}

[[oxc::stage("vertex")]]
float4 mainVS(float2 uv : TEXCOORD0, nointerpolation uint test : KAAS0, Test t) : SV_POSITION {
	return float4(uv * 2 - 1, 0, 1);
}

[shader("pixel")]
float4 mainPS(float4 pos : SV_POSITION) : SV_TARGET {
	return float4(pos.xy, 0, 1);
}

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
// CHECK:                         "Name": "Test",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "pos",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Semantic": "POSITION0",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float3"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "test",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Semantic": "FLAT0",
// CHECK:                                         "Interpolation": "Constant",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "uint"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "testFunction",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "a": {
// CHECK:                                                 "TypeName": "uint",
// CHECK:                                                 "Access": "inout"
// CHECK:                                         },
// CHECK:                                         "b": {
// CHECK:                                                 "TypeName": "uint",
// CHECK:                                                 "Access": "out"
// CHECK:                                         },
// CHECK:                                         "c": {
// CHECK:                                                 "TypeName": "uint",
// CHECK:                                                 "Access": "in"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "mainVS",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Semantic": "SV_POSITION",
// CHECK:                         "Annotations": [
// CHECK:                                 {{"\[\[oxc::stage\(\\\"vertex\\\"\)\]\]"}}
// CHECK:                         ],
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "uv": {
// CHECK:                                                 "TypeName": "float2",
// CHECK:                                                 "Semantic": "TEXCOORD0"
// CHECK:                                         },
// CHECK:                                         "test": {
// CHECK:                                                 "TypeName": "uint",
// CHECK:                                                 "Semantic": "KAAS0",
// CHECK:                                                 "Interpolation": "Constant"
// CHECK:                                         },
// CHECK:                                         "t": {
// CHECK:                                                 "TypeName": "Test"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "float4"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "mainPS",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Semantic": "SV_TARGET",
// CHECK:                         "Annotations": [
// CHECK:                                 "[shader(\"pixel\")]"
// CHECK:                         ],
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "pos": {
// CHECK:                                                 "TypeName": "float4",
// CHECK:                                                 "Semantic": "SV_POSITION"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "float4"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
