// RUN: %dxreflector %s | FileCheck %s

struct A {
	float a;
};

struct B {
	A a;
	float b;
};

struct C {
	B b;
	float c;
};

struct D {
	C c;
	float d;
};

StructuredBuffer<D> buf;
D var;

void test(D d) { }

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
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "B",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "A",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "a",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
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
// CHECK:                         "Name": "C",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "b",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "B",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "a",
// CHECK:                                                                 "TypeName": "A",
// CHECK:                                                                 "Members": [
// CHECK:                                                                         {
// CHECK:                                                                                 "Name": "a",
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         }
// CHECK:                                                                 ]
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "b",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "c",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "D",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "c",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "C",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "b",
// CHECK:                                                                 "TypeName": "B",
// CHECK:                                                                 "Members": [
// CHECK:                                                                         {
// CHECK:                                                                                 "Name": "a",
// CHECK:                                                                                 "TypeName": "A",
// CHECK:                                                                                 "Members": [
// CHECK:                                                                                         {
// CHECK:                                                                                                 "Name": "a",
// CHECK:                                                                                                 "TypeName": "float"
// CHECK:                                                                                         }
// CHECK:                                                                                 ]
// CHECK:                                                                         },
// CHECK:                                                                         {
// CHECK:                                                                                 "Name": "b",
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         }
// CHECK:                                                                 ]
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "d",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
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
// CHECK:                                                 "Name": "D",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "TypeName": "C",
// CHECK:                                                                 "Members": [
// CHECK:                                                                         {
// CHECK:                                                                                 "Name": "b",
// CHECK:                                                                                 "TypeName": "B",
// CHECK:                                                                                 "Members": [
// CHECK:                                                                                         {
// CHECK:                                                                                                 "Name": "a",
// CHECK:                                                                                                 "TypeName": "A",
// CHECK:                                                                                                 "Members": [
// CHECK:                                                                                                         {
// CHECK:                                                                                                                 "Name": "a",
// CHECK:                                                                                                                 "TypeName": "float"
// CHECK:                                                                                                         }
// CHECK:                                                                                                 ]
// CHECK:                                                                                         },
// CHECK:                                                                                         {
// CHECK:                                                                                                 "Name": "b",
// CHECK:                                                                                                 "TypeName": "float"
// CHECK:                                                                                         }
// CHECK:                                                                                 ]
// CHECK:                                                                         },
// CHECK:                                                                         {
// CHECK:                                                                                 "Name": "c",
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         }
// CHECK:                                                                 ]
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "d",
// CHECK:                                                                 "TypeName": "float"
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
// CHECK:                                 "Name": "D",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "c",
// CHECK:                                                 "TypeName": "C",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "b",
// CHECK:                                                                 "TypeName": "B",
// CHECK:                                                                 "Members": [
// CHECK:                                                                         {
// CHECK:                                                                                 "Name": "a",
// CHECK:                                                                                 "TypeName": "A",
// CHECK:                                                                                 "Members": [
// CHECK:                                                                                         {
// CHECK:                                                                                                 "Name": "a",
// CHECK:                                                                                                 "TypeName": "float"
// CHECK:                                                                                         }
// CHECK:                                                                                 ]
// CHECK:                                                                         },
// CHECK:                                                                         {
// CHECK:                                                                                 "Name": "b",
// CHECK:                                                                                 "TypeName": "float"
// CHECK:                                                                         }
// CHECK:                                                                 ]
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "TypeName": "float"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Name": "d",
// CHECK:                                                 "TypeName": "float"
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "d": {
// CHECK:                                                 "TypeName": "D"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
