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
void test(A a, float b) {
	if(b) {}
	else {}
}

namespace tst {
	float a;
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
// CHECK:         "Strings": [
// CHECK:                 "",
// CHECK:                 "float",
// CHECK:                 "{{.*}}raw_data_test.hlsl",
// CHECK:                 "B",
// CHECK:                 "a",
// CHECK:                 "Test",
// CHECK:                 "A",
// CHECK:                 "C",
// CHECK:                 "test",
// CHECK:                 "b",
// CHECK:                 "tst"
// CHECK:         ],
// CHECK:         "StringsNonDebug": [
// CHECK:                 "oxc::fancy"
// CHECK:         ],
// CHECK:         "Sources": [
// CHECK:                 "{{.*}}raw_data_test.hlsl"
// CHECK:         ],
// CHECK:         "SourcesAsId": [
// CHECK:                 2
// CHECK:         ],
// CHECK:         "Nodes": [
// CHECK:                 {
// CHECK:                         "NodeId": 0,
// CHECK:                         "NodeType": "Namespace",
// CHECK:                         "NodeTypeId": 4,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": -1,
// CHECK:                         "ChildCount": 17,
// CHECK:                         "ChildStart": 1
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 1,
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "NodeTypeId": 6,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 0
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 2,
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "NodeTypeId": 5,
// CHECK:                         "LocalId": 1,
// CHECK:                         "ParentId": 0
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 3,
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "NodeTypeId": 2,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 0,
// CHECK:                         "ChildCount": 2,
// CHECK:                         "ChildStart": 4
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 4,
// CHECK:                         "NodeType": "EnumValue",
// CHECK:                         "NodeTypeId": 3,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 3
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 5,
// CHECK:                         "NodeType": "EnumValue",
// CHECK:                         "NodeTypeId": 3,
// CHECK:                         "LocalId": 1,
// CHECK:                         "ParentId": 3
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 6,
// CHECK:                         "NodeType": "Interface",
// CHECK:                         "NodeTypeId": 10,
// CHECK:                         "LocalId": 2,
// CHECK:                         "ParentId": 0
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 7,
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "NodeTypeId": 7,
// CHECK:                         "LocalId": 3,
// CHECK:                         "ParentId": 0,
// CHECK:                         "ChildCount": 2,
// CHECK:                         "ChildStart": 8
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 8,
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "NodeTypeId": 5,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 7
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 9,
// CHECK:                         "NodeType": "Function",
// CHECK:                         "NodeTypeId": 1,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 7
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 10,
// CHECK:                         "NodeType": "Register",
// CHECK:                         "NodeTypeId": 0,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 0,
// CHECK:                         "ChildCount": 1,
// CHECK:                         "ChildStart": 11
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 11,
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "NodeTypeId": 5,
// CHECK:                         "LocalId": 4,
// CHECK:                         "ParentId": 10
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 12,
// CHECK:                         "NodeType": "Function",
// CHECK:                         "NodeTypeId": 1,
// CHECK:                         "LocalId": 1,
// CHECK:                         "ParentId": 0,
// CHECK:                         "ChildCount": 3,
// CHECK:                         "ChildStart": 13,
// CHECK:                         "AnnotationStart": 0,
// CHECK:                         "AnnotationCount": 1,
// CHECK:                         "Annotations": [
// CHECK:                                 "{{\[\[oxc::fancy\]\]}}"
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 13,
// CHECK:                         "NodeType": "Parameter",
// CHECK:                         "NodeTypeId": 11,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 12
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 14,
// CHECK:                         "NodeType": "Parameter",
// CHECK:                         "NodeTypeId": 11,
// CHECK:                         "LocalId": 1,
// CHECK:                         "ParentId": 12
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 15,
// CHECK:                         "NodeType": "If",
// CHECK:                         "NodeTypeId": 12,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 12
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 16,
// CHECK:                         "NodeType": "Namespace",
// CHECK:                         "NodeTypeId": 4,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 0,
// CHECK:                         "ChildCount": 1,
// CHECK:                         "ChildStart": 17
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "NodeId": 17,
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "NodeTypeId": 5,
// CHECK:                         "LocalId": 0,
// CHECK:                         "ParentId": 16
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Registers": [
// CHECK:                 {
// CHECK:                         "RegisterId": 0,
// CHECK:                         "NodeId": 10,
// CHECK:                         "Name": "b",
// CHECK:                         "RegisterType": "cbuffer",
// CHECK:                         "BufferId": 0
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Functions": [
// CHECK:                 {
// CHECK:                         "FunctionId": 0,
// CHECK:                         "NodeId": 9,
// CHECK:                         "Name": "test",
// CHECK:                         "HasDefinition": true,
// CHECK:                         "Params": {
// CHECK:                         },
// CHECK:                         "ReturnType": {
// CHECK:                                 "TypeName": "void"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "FunctionId": 1,
// CHECK:                         "NodeId": 12,
// CHECK:                         "Name": "test",
// CHECK:                         "HasDefinition": true,
// CHECK:                         "Params": {
// CHECK:                                 "a": {
// CHECK:                                         "TypeId": 5,
// CHECK:                                         "TypeName": "A"
// CHECK:                                 },
// CHECK:                                 "b": {
// CHECK:                                         "TypeId": 0,
// CHECK:                                         "TypeName": "float"
// CHECK:                                 }
// CHECK:                         },
// CHECK:                         "ReturnType": {
// CHECK:                                 "TypeName": "void"
// CHECK:                         }
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Parameters": [
// CHECK:                 {
// CHECK:                         "ParamName": "a",
// CHECK:                         "TypeId": 5,
// CHECK:                         "TypeName": "A"
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "ParamName": "b",
// CHECK:                         "TypeId": 0,
// CHECK:                         "TypeName": "float"
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Enums": [
// CHECK:                 {
// CHECK:                         "EnumId": 0,
// CHECK:                         "NodeId": 3,
// CHECK:                         "Name": "Test",
// CHECK:                         "EnumType": "int",
// CHECK:                         "Values": [
// CHECK:                                 {
// CHECK:                                         "ValueId": 0,
// CHECK:                                         "Value": 0,
// CHECK:                                         "Symbol": {
// CHECK:                                                 "Name": "A",
// CHECK:                                                 "NameId": 6
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "ValueId": 1,
// CHECK:                                         "Value": 1,
// CHECK:                                         "Symbol": {
// CHECK:                                                 "Name": "B",
// CHECK:                                                 "NameId": 3
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ],
// CHECK:         "EnumValues": [
// CHECK:                 {
// CHECK:                         "Value": 0,
// CHECK:                         "Symbol": {
// CHECK:                                 "Name": "A",
// CHECK:                                 "NameId": 6
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Value": 1,
// CHECK:                         "Symbol": {
// CHECK:                                 "Name": "B",
// CHECK:                                 "NameId": 3
// CHECK:                         }
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Annotations": [
// CHECK:                 {
// CHECK:                         "StringId": 0,
// CHECK:                         "Contents": "oxc::fancy",
// CHECK:                         "Type": "User"
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Arrays": [
// CHECK:                 {
// CHECK:                         "ArrayElem": 2,
// CHECK:                         "ArrayStart": 0,
// CHECK:                         "ArraySizes": [
// CHECK:                                 2,
// CHECK:                                 3
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "ArrayElem": 2,
// CHECK:                         "ArrayStart": 2,
// CHECK:                         "ArraySizes": [
// CHECK:                                 2,
// CHECK:                                 3
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ],
// CHECK:         "ArraySizes": [
// CHECK:                 2,
// CHECK:                 3,
// CHECK:                 2,
// CHECK:                 3
// CHECK:         ],
// CHECK:         "Members": [
// CHECK:                 {
// CHECK:                         "Name": "a",
// CHECK:                         "NameId": 4,
// CHECK:                         "TypeId": 0,
// CHECK:                         "TypeName": "float"
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "a",
// CHECK:                         "NameId": 4,
// CHECK:                         "TypeId": 0,
// CHECK:                         "TypeName": "float"
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "a",
// CHECK:                         "NameId": 4,
// CHECK:                         "TypeId": 0,
// CHECK:                         "TypeName": "float"
// CHECK:                 }
// CHECK:         ],
// CHECK:         "TypeList": [
// CHECK:                 {
// CHECK:                         "TypeId": 2,
// CHECK:                         "Name": "C"
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Types": [
// CHECK:                 {
// CHECK:                         "TypeId": 0,
// CHECK:                         "Name": "float"
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "TypeId": 1,
// CHECK:                         "Name": "float",
// CHECK:                         "ArraySize": [
// CHECK:                                 2,
// CHECK:                                 3
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "TypeId": 2,
// CHECK:                         "Name": "C"
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "TypeId": 3,
// CHECK:                         "Name": "A",
// CHECK:                         "Interfaces": [
// CHECK:                                 {
// CHECK:                                         "TypeId": 2,
// CHECK:                                         "Name": "C"
// CHECK:                                 }
// CHECK:                         ],
// CHECK:                         "Members": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NameId": 4,
// CHECK:                                         "TypeId": 0,
// CHECK:                                         "TypeName": "float"
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "TypeId": 4,
// CHECK:                         "Name": "A",
// CHECK:                         "Interfaces": [
// CHECK:                                 {
// CHECK:                                         "TypeId": 2,
// CHECK:                                         "Name": "C"
// CHECK:                                 }
// CHECK:                         ],
// CHECK:                         "Members": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NameId": 4,
// CHECK:                                         "TypeId": 0,
// CHECK:                                         "TypeName": "float"
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "TypeId": 5,
// CHECK:                         "Name": "A",
// CHECK:                         "Interfaces": [
// CHECK:                                 {
// CHECK:                                         "TypeId": 2,
// CHECK:                                         "Name": "C"
// CHECK:                                 }
// CHECK:                         ],
// CHECK:                         "Members": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NameId": 4,
// CHECK:                                         "TypeId": 0,
// CHECK:                                         "TypeName": "float"
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Buffers": [
// CHECK:                 {
// CHECK:                         "BufferId": 0,
// CHECK:                         "NodeId": 10,
// CHECK:                         "Name": "b",
// CHECK:                         "Type": "cbuffer",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "NodeId": 11,
// CHECK:                                         "ChildId": 0,
// CHECK:                                         "Name": "b",
// CHECK:                                         "TypeId": 4,
// CHECK:                                         "TypeName": "A",
// CHECK:                                         "Interfaces": [
// CHECK:                                                 {
// CHECK:                                                         "TypeId": 2,
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         ],
// CHECK:                                         "Members": [
// CHECK:                                                 {
// CHECK:                                                         "Name": "a",
// CHECK:                                                         "NameId": 4,
// CHECK:                                                         "TypeId": 0,
// CHECK:                                                         "TypeName": "float"
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ],
// CHECK:         "Statements": [
// CHECK:                 {
// CHECK:                         "Type": "If",
// CHECK:                         "NodeId": 15,
// CHECK:                         "HasConditionVar": false,
// CHECK:                         "HasElse": true
// CHECK:                 }
// CHECK:         ]
// CHECK: }
