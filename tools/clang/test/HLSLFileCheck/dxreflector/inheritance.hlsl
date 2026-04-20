// RUN: %dxreflector %s | FileCheck %s

struct A {
	float4 color1 : COLOR1;  
};

interface B { };

interface C {
	float4 test();
};

struct PSInput : A, B, C {
	float4 color : COLOR;
	float4 test() { return 2; }
};

StructuredBuffer<PSInput> buf;
PSInput var;

float4 PSMain(PSInput input) : SV_TARGET {
	return input.color1 * input.color * buf[0].color;
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
// CHECK:                         "Name": "A",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "color1",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Semantic": "COLOR1",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float4"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "B",
// CHECK:                         "NodeType": "Interface"
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "C",
// CHECK:                         "NodeType": "Interface",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "test",
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "HasDefinition": false,
// CHECK:                                                 "Params": {
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeName": "float4"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "PSInput",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Class": {
// CHECK:                                 "BaseClass": {
// CHECK:                                         "Name": "A"
// CHECK:                                 },
// CHECK:                                 "Interfaces": [
// CHECK:                                         {
// CHECK:                                                 "Name": "B"
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Name": "C"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "color",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Semantic": "COLOR",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float4"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "test",
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "Params": {
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeName": "float4"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "buf",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "PSInput",
// CHECK:                                                 "BaseClass": {
// CHECK:                                                         "TypeName": "A",
// CHECK:                                                         "Members": [
// CHECK:                                                                 {
// CHECK:                                                                         "Name": "color1",
// CHECK:                                                                         "TypeName": "float4"
// CHECK:                                                                 }
// CHECK:                                                         ]
// CHECK:                                                 },
// CHECK:                                                 "Interfaces": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "B"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "C"
// CHECK:                                                         }
// CHECK:                                                 ],
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "color",
// CHECK:                                                                 "TypeName": "float4"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "var",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "PSInput",
// CHECK:                                 "BaseClass": {
// CHECK:                                         "TypeName": "A",
// CHECK:                                         "Members": [
// CHECK:                                                 {
// CHECK:                                                         "Name": "color1",
// CHECK:                                                         "TypeName": "float4"
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 "Interfaces": [
// CHECK:                                         {
// CHECK:                                                 "Name": "B"
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Name": "C"
// CHECK:                                         }
// CHECK:                                 ],
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "color",
// CHECK:                                                 "TypeName": "float4"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "PSMain",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Semantic": "SV_TARGET",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "input": {
// CHECK:                                                 "TypeName": "PSInput"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "float4"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
