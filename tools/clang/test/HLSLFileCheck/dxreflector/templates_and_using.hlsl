// RUN: %dxreflector %s | FileCheck %s

//TODO: Expose these to the AST, now they get ignored 
// unless they get instantiated in a variable, struct member or function param
// This means you have to be a bit careful when walking the tree 
// since some things might be missing.
template<typename T, uint N>
struct ForSomeReasonAVector {

	T data[N];

	template<typename T2, uint M>
	void add(ForSomeReasonAVector<T2, M> v) {
		for(uint i = 0; i < M && i < N; ++i)
			data[i] += v.data[i];
	}
};

template<typename T2, uint M>
ForSomeReasonAVector<T2, M> pow2(ForSomeReasonAVector<T2, M> v) {
	for(uint i = 0; i < M; ++i)
		v.data[i] *= v.data[i];
}

template<uint N>
using Test = float[N];

//Supported types using a using or template type

typedef ForSomeReasonAVector<float, 4> F32x4;
typedef ForSomeReasonAVector<uint, 2> U32x2;
typedef Test<4> Test4;
typedef Test<2> Test2;

struct Struct {
	ForSomeReasonAVector<float, 4> a;
	ForSomeReasonAVector<uint, 2> b;
	Test<4> c;
	Test<2> d;
	F32x4 e;
	U32x2 f;
	Test4 g;
	Test2 h;
};

ForSomeReasonAVector<float, 4> a;
ForSomeReasonAVector<uint, 2> b;
Test<4> c;
Test<2> d;
F32x4 e;
U32x2 f;
Test4 g;
Test2 h;

void func(
	ForSomeReasonAVector<float, 4> a,
	ForSomeReasonAVector<uint, 2> b,
	Test<4> c,
	Test<2> d,
	F32x4 e,
	U32x2 f,
	Test4 g,
	Test2 h
) { }

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
// CHECK:                         "Name": "F32x4",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "ForSomeReasonAVector<float, 4>",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "data",
// CHECK:                                                 "TypeName": "float",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "U32x2",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "ForSomeReasonAVector<uint, 2>",
// CHECK:                                 "UnderlyingName": "ForSomeReasonAVector<unsigned int, 2>",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "data",
// CHECK:                                                 "TypeName": "unsigned int",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test4",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test<4>",
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test2",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test<2>",
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Struct",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "ForSomeReasonAVector<float, 4>",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "data",
// CHECK:                                                                 "TypeName": "float",
// CHECK:                                                                 "ArraySize": [
// CHECK:                                                                         4
// CHECK:                                                                 ]
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "b",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "ForSomeReasonAVector<uint, 2>",
// CHECK:                                                 "UnderlyingName": "ForSomeReasonAVector<unsigned int, 2>",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "data",
// CHECK:                                                                 "TypeName": "unsigned int",
// CHECK:                                                                 "ArraySize": [
// CHECK:                                                                         2
// CHECK:                                                                 ]
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "c",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test<4>",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "d",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test<2>",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "e",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "F32x4",
// CHECK:                                                 "UnderlyingName": "ForSomeReasonAVector<float, 4>",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "data",
// CHECK:                                                                 "TypeName": "float",
// CHECK:                                                                 "ArraySize": [
// CHECK:                                                                         4
// CHECK:                                                                 ]
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "f",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "U32x2",
// CHECK:                                                 "UnderlyingName": "ForSomeReasonAVector<unsigned int, 2>",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "data",
// CHECK:                                                                 "TypeName": "unsigned int",
// CHECK:                                                                 "ArraySize": [
// CHECK:                                                                         2
// CHECK:                                                                 ]
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "g",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test4",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "h",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test2",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "a",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "ForSomeReasonAVector<float, 4>",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "data",
// CHECK:                                                 "TypeName": "float",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "b",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "ForSomeReasonAVector<uint, 2>",
// CHECK:                                 "UnderlyingName": "ForSomeReasonAVector<unsigned int, 2>",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "data",
// CHECK:                                                 "TypeName": "unsigned int",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "c",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test<4>",
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "d",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test<2>",
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "e",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "F32x4",
// CHECK:                                 "UnderlyingName": "ForSomeReasonAVector<float, 4>",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "data",
// CHECK:                                                 "TypeName": "float",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "U32x2",
// CHECK:                                 "UnderlyingName": "ForSomeReasonAVector<unsigned int, 2>",
// CHECK:                                 "Members": [
// CHECK:                                         {
// CHECK:                                                 "Name": "data",
// CHECK:                                                 "TypeName": "unsigned int",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "g",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test4",
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "h",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test2",
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "func",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "a": {
// CHECK:                                                 "TypeName": "ForSomeReasonAVector<float, 4>"
// CHECK:                                         },
// CHECK:                                         "b": {
// CHECK:                                                 "TypeName": "ForSomeReasonAVector<uint, 2>",
// CHECK:                                                 "UnderlyingName": "ForSomeReasonAVector<unsigned int, 2>"
// CHECK:                                         },
// CHECK:                                         "c": {
// CHECK:                                                 "TypeName": "Test<4>",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "d": {
// CHECK:                                                 "TypeName": "Test<2>",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "e": {
// CHECK:                                                 "TypeName": "F32x4",
// CHECK:                                                 "UnderlyingName": "ForSomeReasonAVector<float, 4>"
// CHECK:                                         },
// CHECK:                                         "f": {
// CHECK:                                                 "TypeName": "U32x2",
// CHECK:                                                 "UnderlyingName": "ForSomeReasonAVector<unsigned int, 2>"
// CHECK:                                         },
// CHECK:                                         "g": {
// CHECK:                                                 "TypeName": "Test4",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "h": {
// CHECK:                                                 "TypeName": "Test2",
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
