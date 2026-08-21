// RUN: %dxreflector -reflect-disable-symbols %s | FileCheck %s

struct T {
	uint otherData[4];
};

groupshared uint groupData[16];
static const uint staticData[4] = { 0, 1, 2, 3 };
uint dynamicData[4];
StructuredBuffer<T> other;

struct Test {
	static const uint staticData = 0;
	static float adds(float a, float b) { return a + b; }
	float add(float a, float b) { return a + b; }
};

namespace tst {

	static const uint staticData[4] = { 0, 1, 2, 3 };
	float add(float a, float b) { return a + b; }

	struct Test {
		static const uint staticData = 0;
		static float adds(float a, float b) { return a + b; }
		float add(float a, float b) { return a + b; }
	};
}

typedef float f32;

f32 add(float a, float b) { return a + b; }

// CHECK: {
// CHECK:         "Features": [
// CHECK:                 "Basics",
// CHECK:                 "Functions",
// CHECK:                 "Namespaces",
// CHECK:                 "UserTypes",
// CHECK:                 "Scopes"
// CHECK:         ],
// CHECK:         "Children": [
// CHECK:                 {
// CHECK:                         "NodeId": 1,
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "NodeId": 2,
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "TypeId": 0,
// CHECK:                                                 "Name": "uint",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 3,
// CHECK:                         "NodeType": "GroupsharedVariable",
// CHECK:                         "Type": {
// CHECK:                                 "TypeId": 2,
// CHECK:                                 "Name": "uint",
// CHECK:                                 "ArraySize": [
// CHECK:                                         16
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 4,
// CHECK:                         "NodeType": "StaticVariable",
// CHECK:                         "Type": {
// CHECK:                                 "TypeId": 0,
// CHECK:                                 "Name": "uint",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 5,
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "TypeId": 0,
// CHECK:                                 "Name": "uint",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 6,
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterId": 0,
// CHECK:                                 "NodeId": 6,
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "NodeId": 7,
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "TypeId": 3,
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "TypeId": 0,
// CHECK:                                                                 "TypeName": "uint",
// CHECK:                                                                 "ArraySize": [
// CHECK:                                                                         4
// CHECK:                                                                 ]
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 8,
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "NodeId": 9,
// CHECK:                                         "NodeType": "StaticVariable",
// CHECK:                                         "Type": {
// CHECK:                                                 "TypeId": 5,
// CHECK:                                                 "Name": "uint"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeId": 10,
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "FunctionId": 0,
// CHECK:                                                 "NodeId": 10,
// CHECK:                                                 "Params": {
// CHECK:                                                         "0": {
// CHECK:                                                                 "TypeId": 6,
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         },
// CHECK:                                                         "1": {
// CHECK:                                                                 "TypeId": 6,
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeId": 6,
// CHECK:                                                         "TypeName": "float"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeId": 14,
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "FunctionId": 1,
// CHECK:                                                 "NodeId": 14,
// CHECK:                                                 "Params": {
// CHECK:                                                         "0": {
// CHECK:                                                                 "TypeId": 6,
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         },
// CHECK:                                                         "1": {
// CHECK:                                                                 "TypeId": 6,
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeId": 6,
// CHECK:                                                         "TypeName": "float"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 18,
// CHECK:                         "NodeType": "Namespace",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "NodeId": 19,
// CHECK:                                         "NodeType": "StaticVariable",
// CHECK:                                         "Type": {
// CHECK:                                                 "TypeId": 0,
// CHECK:                                                 "Name": "uint",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeId": 20,
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Function": {
// CHECK:                                                 "FunctionId": 2,
// CHECK:                                                 "NodeId": 20,
// CHECK:                                                 "Params": {
// CHECK:                                                         "0": {
// CHECK:                                                                 "TypeId": 6,
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         },
// CHECK:                                                         "1": {
// CHECK:                                                                 "TypeId": 6,
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeId": 6,
// CHECK:                                                         "TypeName": "float"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "NodeId": 24,
// CHECK:                                         "NodeType": "Struct",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "NodeId": 25,
// CHECK:                                                         "NodeType": "StaticVariable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "TypeId": 5,
// CHECK:                                                                 "Name": "uint"
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeId": 26,
// CHECK:                                                         "NodeType": "Function",
// CHECK:                                                         "Function": {
// CHECK:                                                                 "FunctionId": 3,
// CHECK:                                                                 "NodeId": 26,
// CHECK:                                                                 "Params": {
// CHECK:                                                                         "0": {
// CHECK:                                                                                 "TypeId": 6,
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         },
// CHECK:                                                                         "1": {
// CHECK:                                                                                 "TypeId": 6,
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         }
// CHECK:                                                                 },
// CHECK:                                                                 "ReturnType": {
// CHECK:                                                                         "TypeId": 6,
// CHECK:                                                                         "TypeName": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 },
// CHECK:                                                 {
// CHECK:                                                         "NodeId": 30,
// CHECK:                                                         "NodeType": "Function",
// CHECK:                                                         "Function": {
// CHECK:                                                                 "FunctionId": 4,
// CHECK:                                                                 "NodeId": 30,
// CHECK:                                                                 "Params": {
// CHECK:                                                                         "0": {
// CHECK:                                                                                 "TypeId": 6,
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         },
// CHECK:                                                                         "1": {
// CHECK:                                                                                 "TypeId": 6,
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         }
// CHECK:                                                                 },
// CHECK:                                                                 "ReturnType": {
// CHECK:                                                                         "TypeId": 6,
// CHECK:                                                                         "TypeName": "float"
// CHECK:                                                                 }
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 34,
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "TypeId": 6,
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 35,
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "FunctionId": 5,
// CHECK:                                 "NodeId": 35,
// CHECK:                                 "Params": {
// CHECK:                                         "0": {
// CHECK:                                                 "TypeId": 6,
// CHECK:                                                 "TypeName": "float"
// CHECK:                                         },
// CHECK:                                         "1": {
// CHECK:                                                 "TypeId": 6,
// CHECK:                                                 "TypeName": "float"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeId": 6,
// CHECK:                                         "TypeName": "float"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
