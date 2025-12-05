// RUN: %dxreflector %s | FileCheck %s

namespace tst {
	struct Tst;
	struct A {
		float a();
	};
}

struct Test;
struct Test;

struct Never0;

struct A {
	float a;
	float t(Test t);  
	float t2(tst::Tst a); 
	float neverDeclared(Never0 a);
};

struct Test {
	float a;  
};

struct tst::Tst {
	float a;  
};

float A::t(Test t) { return 2; }
float A::t2(tst::Tst a) { return 2; }
float tst::A::a() { return 2; }

float neverDeclared(Never0 a);

float4 test();
float4 test();

float4 test() { return 0.xxxx; }

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
// CHECK:                         "Name": "tst",
// CHECK:                         "NodeType": "Namespace",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "Tst",
// CHECK:                                         "NodeType": "Struct",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "Name": "a",
// CHECK:                                                         "NodeType": "Variable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "float"
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "A",
// CHECK:                                         "NodeType": "Struct",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "Name": "a",
// CHECK:                                                         "NodeType": "Function",
// CHECK:                                                         "Function": {
// CHECK:                                                                 "Params": {
// CHECK:                                                                 },
// CHECK:                                                                 "ReturnType": {
// CHECK:                                                                         "TypeName": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 }
// CHECK:                         ]
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
// CHECK:                         "Name": "Never0",
// CHECK:                         "NodeType": "Struct"
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "A",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "t",
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "Params": {
// CHECK:                                                         "t": {
// CHECK:                                                                 "TypeName": "Test"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeName": "float"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "t2",
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "Params": {
// CHECK:                                                         "a": {
// CHECK:                                                                 "TypeName": "tst::Tst"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeName": "float"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "neverDeclared",
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "HasDefinition": false,
// CHECK:                                                 "Params": {
// CHECK:                                                         "a": {
// CHECK:                                                                 "TypeName": "Never0"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeName": "float"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "neverDeclared",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "HasDefinition": false,
// CHECK:                                 "Params": {
// CHECK:                                         "a": {
// CHECK:                                                 "TypeName": "Never0"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "float"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "float4"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
