// RUN: %dxreflector -reflect-functions %s | FileCheck %s

struct PSInput {
	float4 color : COLOR;
};

struct Test;

struct A {
	float a;
	float t(Test t);  
};

struct Test {
	float a;  
};

float A::t(Test t) {
	return 2;
}

[[oxc::stage("pixel")]]
float4 PSMain(PSInput input) : SV_TARGET
{
	A a;
	Test t;
	return input.color * a.t(t);
}

// CHECK: {
// CHECK:         "Features": [
// CHECK:                 "Functions",
// CHECK:                 "Symbols"
// CHECK:         ],
// CHECK:         "Children": [
// CHECK:                 {
// CHECK:                         "Name": "PSMain",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Semantic": "SV_TARGET",
// CHECK:                         "Annotations": [
// CHECK:                                 {{"\[\[oxc::stage\(\\\"pixel\\\"\)\]\]"}}
// CHECK:                         ],
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