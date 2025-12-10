// RUN: %dxreflector -reflect-show-raw-data %s | FileCheck %s

typedef float B;
float a[2][3];

enum class Test {
	A,
	B
};

interface C {};

struct A : C {
	float a;
	void test() {}
};

ConstantBuffer<A> b;

[[oxc::fancy]]
void test(A a, uint b) {

	if(b) { float a0; }
	else if(b != 1) { float a1; }
	else { float a2; }

	do { float a3; } while(0);
	
	if(uint d = b ^ 23) { float a4; }
	else if(uint d = b ^ 22) { float a5; }
	else { float a6; }

	switch(uint d = b ^ 23) {
		default: { float a7; break; }
	}

	switch(b) {
		case 0: { float a8; break; }
		case 1: { float a9; break; }
		default: break;
	}

	while(false) { float a10; break; }

	for(int i = 0; i < 16; ++i) {
		float a11;
	}
	
	for(int i = 0, j = 0; i < 16; ++i, ++j) {
		float a12;
	}
	
	for(int i = 0, j = 0; bool k = i < 16; ++i, ++j) {
		float a12;
	}

	{ float a13; }
}

namespace tst {
	float a;
}

// CHECK: {
// CHECK: 	"Features": [
// CHECK: 		"Basics",
// CHECK: 		"Functions",
// CHECK: 		"Namespaces",
// CHECK: 		"UserTypes",
// CHECK: 		"Scopes",
// CHECK: 		"Symbols"
// CHECK: 	],
// CHECK: 	"Strings": [
// CHECK: 		"",
// CHECK: 		"float",
// CHECK: 		"{{.*}}raw_data_test.hlsl",
// CHECK: 		"B",
// CHECK: 		"a",
// CHECK: 		"Test",
// CHECK: 		"A",
// CHECK: 		"C",
// CHECK: 		"test",
// CHECK: 		"b",
// CHECK: 		"uint",
// CHECK: 		"a0",
// CHECK: 		"a1",
// CHECK: 		"a2",
// CHECK: 		"a3",
// CHECK: 		"d",
// CHECK: 		"a4",
// CHECK: 		"a5",
// CHECK: 		"a6",
// CHECK: 		"a7",
// CHECK: 		"a8",
// CHECK: 		"a9",
// CHECK: 		"a10",
// CHECK: 		"int",
// CHECK: 		"i",
// CHECK: 		"a11",
// CHECK: 		"j",
// CHECK: 		"a12",
// CHECK: 		"bool",
// CHECK: 		"k",
// CHECK: 		"a13",
// CHECK: 		"tst"
// CHECK: 	],
// CHECK: 	"StringsNonDebug": [
// CHECK: 		"oxc::fancy"
// CHECK: 	],
// CHECK: 	"Sources": [
// CHECK: 		"{{.*}}raw_data_test.hlsl"
// CHECK: 	],
// CHECK: 	"SourcesAsId": [
// CHECK: 		2
// CHECK: 	],
// CHECK: 	"Nodes": [
// CHECK: 		{
// CHECK: 			"NodeId": 0,
// CHECK: 			"NodeType": "Namespace",
// CHECK: 			"NodeTypeId": 4,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": -1,
// CHECK: 			"ChildCount": 60,
// CHECK: 			"ChildStart": 1
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 1,
// CHECK: 			"NodeType": "Typedef",
// CHECK: 			"NodeTypeId": 6,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 0
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 2,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 1,
// CHECK: 			"ParentId": 0
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 3,
// CHECK: 			"NodeType": "Enum",
// CHECK: 			"NodeTypeId": 2,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 0,
// CHECK: 			"ChildCount": 2,
// CHECK: 			"ChildStart": 4
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 4,
// CHECK: 			"NodeType": "EnumValue",
// CHECK: 			"NodeTypeId": 3,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 3
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 5,
// CHECK: 			"NodeType": "EnumValue",
// CHECK: 			"NodeTypeId": 3,
// CHECK: 			"LocalId": 1,
// CHECK: 			"ParentId": 3
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 6,
// CHECK: 			"NodeType": "Interface",
// CHECK: 			"NodeTypeId": 10,
// CHECK: 			"LocalId": 2,
// CHECK: 			"ParentId": 0
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 7,
// CHECK: 			"NodeType": "Struct",
// CHECK: 			"NodeTypeId": 7,
// CHECK: 			"LocalId": 3,
// CHECK: 			"ParentId": 0,
// CHECK: 			"ChildCount": 2,
// CHECK: 			"ChildStart": 8
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 8,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 7
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 9,
// CHECK: 			"NodeType": "Function",
// CHECK: 			"NodeTypeId": 1,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 7
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 10,
// CHECK: 			"NodeType": "Register",
// CHECK: 			"NodeTypeId": 0,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 0,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 11
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 11,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 4,
// CHECK: 			"ParentId": 10
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 12,
// CHECK: 			"NodeType": "Function",
// CHECK: 			"NodeTypeId": 1,
// CHECK: 			"LocalId": 1,
// CHECK: 			"ParentId": 0,
// CHECK: 			"ChildCount": 46,
// CHECK: 			"ChildStart": 13,
// CHECK: 			"AnnotationStart": 0,
// CHECK: 			"AnnotationCount": 1,
// CHECK: 			"Annotations": [
// CHECK: 				"{{\[\[oxc::fancy\]\]}}"
// CHECK: 			]
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 13,
// CHECK: 			"NodeType": "Parameter",
// CHECK: 			"NodeTypeId": 11,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 12
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 14,
// CHECK: 			"NodeType": "Parameter",
// CHECK: 			"NodeTypeId": 11,
// CHECK: 			"LocalId": 1,
// CHECK: 			"ParentId": 12
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 15,
// CHECK: 			"NodeType": "IfRoot",
// CHECK: 			"NodeTypeId": 12,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 6,
// CHECK: 			"ChildStart": 16
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 16,
// CHECK: 			"NodeType": "IfFirst",
// CHECK: 			"NodeTypeId": 22,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 15,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 17
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 17,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 16
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 18,
// CHECK: 			"NodeType": "ElseIf",
// CHECK: 			"NodeTypeId": 23,
// CHECK: 			"LocalId": 1,
// CHECK: 			"ParentId": 15,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 19
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 19,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 18
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 20,
// CHECK: 			"NodeType": "Else",
// CHECK: 			"NodeTypeId": 24,
// CHECK: 			"LocalId": 2,
// CHECK: 			"ParentId": 15,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 21
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 21,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 20
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 22,
// CHECK: 			"NodeType": "Do",
// CHECK: 			"NodeTypeId": 14,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 23
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 23,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 22
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 24,
// CHECK: 			"NodeType": "IfRoot",
// CHECK: 			"NodeTypeId": 12,
// CHECK: 			"LocalId": 1,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 8,
// CHECK: 			"ChildStart": 25
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 25,
// CHECK: 			"NodeType": "IfFirst",
// CHECK: 			"NodeTypeId": 22,
// CHECK: 			"LocalId": 3,
// CHECK: 			"ParentId": 24,
// CHECK: 			"ChildCount": 2,
// CHECK: 			"ChildStart": 26
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 26,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 6,
// CHECK: 			"ParentId": 25
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 27,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 25
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 28,
// CHECK: 			"NodeType": "ElseIf",
// CHECK: 			"NodeTypeId": 23,
// CHECK: 			"LocalId": 4,
// CHECK: 			"ParentId": 24,
// CHECK: 			"ChildCount": 2,
// CHECK: 			"ChildStart": 29
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 29,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 6,
// CHECK: 			"ParentId": 28
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 30,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 28
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 31,
// CHECK: 			"NodeType": "Else",
// CHECK: 			"NodeTypeId": 24,
// CHECK: 			"LocalId": 5,
// CHECK: 			"ParentId": 24,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 32
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 32,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 31
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 33,
// CHECK: 			"NodeType": "Switch",
// CHECK: 			"NodeTypeId": 15,
// CHECK: 			"LocalId": 2,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 3,
// CHECK: 			"ChildStart": 34
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 34,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 6,
// CHECK: 			"ParentId": 33
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 35,
// CHECK: 			"NodeType": "Default",
// CHECK: 			"NodeTypeId": 20,
// CHECK: 			"LocalId": 6,
// CHECK: 			"ParentId": 33,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 36
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 36,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 35
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 37,
// CHECK: 			"NodeType": "Switch",
// CHECK: 			"NodeTypeId": 15,
// CHECK: 			"LocalId": 3,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 5,
// CHECK: 			"ChildStart": 38
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 38,
// CHECK: 			"NodeType": "Case",
// CHECK: 			"NodeTypeId": 19,
// CHECK: 			"LocalId": 7,
// CHECK: 			"ParentId": 37,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 39
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 39,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 38
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 40,
// CHECK: 			"NodeType": "Case",
// CHECK: 			"NodeTypeId": 19,
// CHECK: 			"LocalId": 8,
// CHECK: 			"ParentId": 37,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 41
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 41,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 40
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 42,
// CHECK: 			"NodeType": "Default",
// CHECK: 			"NodeTypeId": 20,
// CHECK: 			"LocalId": 9,
// CHECK: 			"ParentId": 37
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 43,
// CHECK: 			"NodeType": "While",
// CHECK: 			"NodeTypeId": 16,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 44
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 44,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 43
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 45,
// CHECK: 			"NodeType": "For",
// CHECK: 			"NodeTypeId": 17,
// CHECK: 			"LocalId": 1,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 2,
// CHECK: 			"ChildStart": 46
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 46,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 7,
// CHECK: 			"ParentId": 45
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 47,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 45
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 48,
// CHECK: 			"NodeType": "For",
// CHECK: 			"NodeTypeId": 17,
// CHECK: 			"LocalId": 2,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 3,
// CHECK: 			"ChildStart": 49
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 49,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 7,
// CHECK: 			"ParentId": 48
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 50,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 7,
// CHECK: 			"ParentId": 48
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 51,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 48
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 52,
// CHECK: 			"NodeType": "For",
// CHECK: 			"NodeTypeId": 17,
// CHECK: 			"LocalId": 3,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 4,
// CHECK: 			"ChildStart": 53
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 53,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 8,
// CHECK: 			"ParentId": 52
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 54,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 7,
// CHECK: 			"ParentId": 52
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 55,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 7,
// CHECK: 			"ParentId": 52
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 56,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 52
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 57,
// CHECK: 			"NodeType": "Scope",
// CHECK: 			"NodeTypeId": 13,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 12,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 58
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 58,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 57
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 59,
// CHECK: 			"NodeType": "Namespace",
// CHECK: 			"NodeTypeId": 4,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 0,
// CHECK: 			"ChildCount": 1,
// CHECK: 			"ChildStart": 60
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"NodeId": 60,
// CHECK: 			"NodeType": "Variable",
// CHECK: 			"NodeTypeId": 5,
// CHECK: 			"LocalId": 0,
// CHECK: 			"ParentId": 59
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Registers": [
// CHECK: 		{
// CHECK: 			"RegisterId": 0,
// CHECK: 			"NodeId": 10,
// CHECK: 			"Name": "b",
// CHECK: 			"RegisterType": "cbuffer",
// CHECK: 			"BufferId": 0
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Functions": [
// CHECK: 		{
// CHECK: 			"FunctionId": 0,
// CHECK: 			"NodeId": 9,
// CHECK: 			"Name": "test",
// CHECK: 			"HasDefinition": true,
// CHECK: 			"Params": {
// CHECK: 			},
// CHECK: 			"ReturnType": {
// CHECK: 				"TypeName": "void"
// CHECK: 			}
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"FunctionId": 1,
// CHECK: 			"NodeId": 12,
// CHECK: 			"Name": "test",
// CHECK: 			"HasDefinition": true,
// CHECK: 			"Params": {
// CHECK: 				"a": {
// CHECK: 					"TypeId": 5,
// CHECK: 					"TypeName": "A"
// CHECK: 				},
// CHECK: 				"b": {
// CHECK: 					"TypeId": 6,
// CHECK: 					"TypeName": "uint"
// CHECK: 				}
// CHECK: 			},
// CHECK: 			"ReturnType": {
// CHECK: 				"TypeName": "void"
// CHECK: 			}
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Parameters": [
// CHECK: 		{
// CHECK: 			"ParamName": "a",
// CHECK: 			"TypeId": 5,
// CHECK: 			"TypeName": "A"
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"ParamName": "b",
// CHECK: 			"TypeId": 6,
// CHECK: 			"TypeName": "uint"
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Enums": [
// CHECK: 		{
// CHECK: 			"EnumId": 0,
// CHECK: 			"NodeId": 3,
// CHECK: 			"Name": "Test",
// CHECK: 			"EnumType": "int",
// CHECK: 			"Values": [
// CHECK: 				{
// CHECK: 					"ValueId": 0,
// CHECK: 					"Value": 0,
// CHECK: 					"Symbol": {
// CHECK: 						"Name": "A",
// CHECK: 						"NameId": 6
// CHECK: 					}
// CHECK: 				},
// CHECK: 				{
// CHECK: 					"ValueId": 1,
// CHECK: 					"Value": 1,
// CHECK: 					"Symbol": {
// CHECK: 						"Name": "B",
// CHECK: 						"NameId": 3
// CHECK: 					}
// CHECK: 				}
// CHECK: 			]
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"EnumValues": [
// CHECK: 		{
// CHECK: 			"Value": 0,
// CHECK: 			"Symbol": {
// CHECK: 				"Name": "A",
// CHECK: 				"NameId": 6
// CHECK: 			}
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Value": 1,
// CHECK: 			"Symbol": {
// CHECK: 				"Name": "B",
// CHECK: 				"NameId": 3
// CHECK: 			}
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Annotations": [
// CHECK: 		{
// CHECK: 			"StringId": 0,
// CHECK: 			"Contents": "oxc::fancy",
// CHECK: 			"Type": "User"
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Arrays": [
// CHECK: 		{
// CHECK: 			"ArrayElem": 2,
// CHECK: 			"ArrayStart": 0,
// CHECK: 			"ArraySizes": [
// CHECK: 				2,
// CHECK: 				3
// CHECK: 			]
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"ArrayElem": 2,
// CHECK: 			"ArrayStart": 2,
// CHECK: 			"ArraySizes": [
// CHECK: 				2,
// CHECK: 				3
// CHECK: 			]
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"ArraySizes": [
// CHECK: 		2,
// CHECK: 		3,
// CHECK: 		2,
// CHECK: 		3
// CHECK: 	],
// CHECK: 	"Members": [
// CHECK: 		{
// CHECK: 			"Name": "a",
// CHECK: 			"NameId": 4,
// CHECK: 			"TypeId": 0,
// CHECK: 			"TypeName": "float"
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Name": "a",
// CHECK: 			"NameId": 4,
// CHECK: 			"TypeId": 0,
// CHECK: 			"TypeName": "float"
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Name": "a",
// CHECK: 			"NameId": 4,
// CHECK: 			"TypeId": 0,
// CHECK: 			"TypeName": "float"
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"TypeList": [
// CHECK: 		{
// CHECK: 			"TypeId": 2,
// CHECK: 			"Name": "C"
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Types": [
// CHECK: 		{
// CHECK: 			"TypeId": 0,
// CHECK: 			"Name": "float"
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 1,
// CHECK: 			"Name": "float",
// CHECK: 			"ArraySize": [
// CHECK: 				2,
// CHECK: 				3
// CHECK: 			]
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 2,
// CHECK: 			"Name": "C"
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 3,
// CHECK: 			"Name": "A",
// CHECK: 			"Interfaces": [
// CHECK: 				{
// CHECK: 					"TypeId": 2,
// CHECK: 					"Name": "C"
// CHECK: 				}
// CHECK: 			],
// CHECK: 			"Members": [
// CHECK: 				{
// CHECK: 					"Name": "a",
// CHECK: 					"NameId": 4,
// CHECK: 					"TypeId": 0,
// CHECK: 					"TypeName": "float"
// CHECK: 				}
// CHECK: 			]
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 4,
// CHECK: 			"Name": "A",
// CHECK: 			"Interfaces": [
// CHECK: 				{
// CHECK: 					"TypeId": 2,
// CHECK: 					"Name": "C"
// CHECK: 				}
// CHECK: 			],
// CHECK: 			"Members": [
// CHECK: 				{
// CHECK: 					"Name": "a",
// CHECK: 					"NameId": 4,
// CHECK: 					"TypeId": 0,
// CHECK: 					"TypeName": "float"
// CHECK: 				}
// CHECK: 			]
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 5,
// CHECK: 			"Name": "A",
// CHECK: 			"Interfaces": [
// CHECK: 				{
// CHECK: 					"TypeId": 2,
// CHECK: 					"Name": "C"
// CHECK: 				}
// CHECK: 			],
// CHECK: 			"Members": [
// CHECK: 				{
// CHECK: 					"Name": "a",
// CHECK: 					"NameId": 4,
// CHECK: 					"TypeId": 0,
// CHECK: 					"TypeName": "float"
// CHECK: 				}
// CHECK: 			]
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 6,
// CHECK: 			"Name": "uint"
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 7,
// CHECK: 			"Name": "int"
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"TypeId": 8,
// CHECK: 			"Name": "bool"
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Buffers": [
// CHECK: 		{
// CHECK: 			"BufferId": 0,
// CHECK: 			"NodeId": 10,
// CHECK: 			"Name": "b",
// CHECK: 			"Type": "cbuffer",
// CHECK: 			"Children": [
// CHECK: 				{
// CHECK: 					"NodeId": 11,
// CHECK: 					"ChildId": 0,
// CHECK: 					"Name": "b",
// CHECK: 					"TypeId": 4,
// CHECK: 					"TypeName": "A",
// CHECK: 					"Interfaces": [
// CHECK: 						{
// CHECK: 							"TypeId": 2,
// CHECK: 							"Name": "C"
// CHECK: 						}
// CHECK: 					],
// CHECK: 					"Members": [
// CHECK: 						{
// CHECK: 							"Name": "a",
// CHECK: 							"NameId": 4,
// CHECK: 							"TypeId": 0,
// CHECK: 							"TypeName": "float"
// CHECK: 						}
// CHECK: 					]
// CHECK: 				}
// CHECK: 			]
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"Statements": [
// CHECK: 		{
// CHECK: 			"Type": "While",
// CHECK: 			"NodeId": 43,
// CHECK: 			"Body": 1
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "For",
// CHECK: 			"NodeId": 45,
// CHECK: 			"Init": 1,
// CHECK: 			"Body": 1
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "For",
// CHECK: 			"NodeId": 48,
// CHECK: 			"Init": 2,
// CHECK: 			"Body": 1
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "For",
// CHECK: 			"NodeId": 52,
// CHECK: 			"HasConditionVar": true,
// CHECK: 			"Init": 2,
// CHECK: 			"Body": 1
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"IfSwitchStatements": [
// CHECK: 		{
// CHECK: 			"Type": "IfRoot",
// CHECK: 			"NodeId": 15,
// CHECK: 			"HasElseOrDefault": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "IfRoot",
// CHECK: 			"NodeId": 24,
// CHECK: 			"HasElseOrDefault": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Switch",
// CHECK: 			"NodeId": 33,
// CHECK: 			"HasConditionVar": true,
// CHECK: 			"HasElseOrDefault": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Switch",
// CHECK: 			"NodeId": 37,
// CHECK: 			"HasElseOrDefault": true
// CHECK: 		}
// CHECK: 	],
// CHECK: 	"BranchStatements": [
// CHECK: 		{
// CHECK: 			"Type": "IfFirst",
// CHECK: 			"NodeId": 16,
// CHECK: 			"IsComplexCase": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "ElseIf",
// CHECK: 			"NodeId": 18,
// CHECK: 			"IsComplexCase": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Else",
// CHECK: 			"NodeId": 20,
// CHECK: 			"IsComplexCase": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "IfFirst",
// CHECK: 			"NodeId": 25,
// CHECK: 			"HasConditionVar": true,
// CHECK: 			"IsComplexCase": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "ElseIf",
// CHECK: 			"NodeId": 28,
// CHECK: 			"HasConditionVar": true,
// CHECK: 			"IsComplexCase": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Else",
// CHECK: 			"NodeId": 31,
// CHECK: 			"IsComplexCase": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Default",
// CHECK: 			"NodeId": 35,
// CHECK: 			"IsComplexCase": true
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Case",
// CHECK: 			"NodeId": 38,
// CHECK: 			"ValueType": "uint",
// CHECK: 			"Value": 0
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Case",
// CHECK: 			"NodeId": 40,
// CHECK: 			"ValueType": "uint",
// CHECK: 			"Value": 1
// CHECK: 		},
// CHECK: 		{
// CHECK: 			"Type": "Default",
// CHECK: 			"NodeId": 42,
// CHECK: 			"IsComplexCase": true
// CHECK: 		}
// CHECK: 	]
// CHECK: }
