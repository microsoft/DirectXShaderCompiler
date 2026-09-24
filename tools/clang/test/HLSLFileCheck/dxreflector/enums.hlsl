// RUN: %dxreflector -T lib_6_4 -enable-16bit-types %s | FileCheck %s

enum Test {
	Test_A,
	Test_B,
	Test_C
};

enum class Test2 {
	A,
	B,
	C
};

enum class Test3 : int {
	A,
	B,
	C
};

enum class Test4 : uint64_t {
	A,
	B,
	C
};

enum class Test5 : int64_t {
	A,
	B,
	C
};

enum class Test6 : uint {
	A,
	B,
	C
};

enum class Test7 : int16_t {
	A,
	B,
	C
};

enum class Test8 : uint16_t {
	A,
	B,
	C
};

Test a;
Test2 b;
Test3 c;
Test4 d;
Test5 e;
Test6 f;
Test7 g;
Test8 h;

struct A {
	Test a;
	Test2 b;
	Test3 c;
	Test4 d;
	Test5 e;
	Test6 f;
	Test7 g;
	Test8 h;
};

StructuredBuffer<A> sbuf;

void test(
	Test a,
	Test2 b,
	Test3 c,
	Test4 d,
	Test5 e,
	Test6 f,
	Test7 g,
	Test8 h) {}
	
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
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test",
// CHECK:                                 "EnumType": "uint",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "Test_A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "Test_B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "Test_C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test2",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test2",
// CHECK:                                 "EnumType": "int",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test3",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test3",
// CHECK:                                 "EnumType": "int",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test4",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test4",
// CHECK:                                 "EnumType": "uint64_t",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test5",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test5",
// CHECK:                                 "EnumType": "int64_t",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test6",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test6",
// CHECK:                                 "EnumType": "uint",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test7",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test7",
// CHECK:                                 "EnumType": "int16_t",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test8",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test8",
// CHECK:                                 "EnumType": "uint16_t",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B"
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 2,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "C"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "a",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test",
// CHECK:                                 "UnderlyingName": "uint"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "b",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test2",
// CHECK:                                 "UnderlyingName": "int"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "c",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test3",
// CHECK:                                 "UnderlyingName": "int"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "d",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test4",
// CHECK:                                 "UnderlyingName": "uint64_t"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "e",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test5",
// CHECK:                                 "UnderlyingName": "int64_t"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test6",
// CHECK:                                 "UnderlyingName": "uint"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "g",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test7",
// CHECK:                                 "UnderlyingName": "int16_t"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "h",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "Test8",
// CHECK:                                 "UnderlyingName": "uint16_t"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "A",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test",
// CHECK:                                                 "UnderlyingName": "uint"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "b",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test2",
// CHECK:                                                 "UnderlyingName": "int"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "c",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test3",
// CHECK:                                                 "UnderlyingName": "int"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "d",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test4",
// CHECK:                                                 "UnderlyingName": "uint64_t"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "e",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test5",
// CHECK:                                                 "UnderlyingName": "int64_t"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "f",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test6",
// CHECK:                                                 "UnderlyingName": "uint"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "g",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test7",
// CHECK:                                                 "UnderlyingName": "int16_t"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "h",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "Test8",
// CHECK:                                                 "UnderlyingName": "uint16_t"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "sbuf",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "StructuredBuffer"
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "$Element",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "A",
// CHECK:                                                 "Members": [
// CHECK:                                                         {
// CHECK:                                                                 "Name": "a",
// CHECK:                                                                 "TypeName": "Test",
// CHECK:                                                                 "UnderlyingName": "uint"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "b",
// CHECK:                                                                 "TypeName": "Test2",
// CHECK:                                                                 "UnderlyingName": "int"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "c",
// CHECK:                                                                 "TypeName": "Test3",
// CHECK:                                                                 "UnderlyingName": "int"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "d",
// CHECK:                                                                 "TypeName": "Test4",
// CHECK:                                                                 "UnderlyingName": "uint64_t"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "e",
// CHECK:                                                                 "TypeName": "Test5",
// CHECK:                                                                 "UnderlyingName": "int64_t"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "f",
// CHECK:                                                                 "TypeName": "Test6",
// CHECK:                                                                 "UnderlyingName": "uint"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "g",
// CHECK:                                                                 "TypeName": "Test7",
// CHECK:                                                                 "UnderlyingName": "int16_t"
// CHECK:                                                         },
// CHECK:                                                         {
// CHECK:                                                                 "Name": "h",
// CHECK:                                                                 "TypeName": "Test8",
// CHECK:                                                                 "UnderlyingName": "uint16_t"
// CHECK:                                                         }
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "a": {
// CHECK:                                                 "TypeName": "Test",
// CHECK:                                                 "UnderlyingName": "uint"
// CHECK:                                         },
// CHECK:                                         "b": {
// CHECK:                                                 "TypeName": "Test2",
// CHECK:                                                 "UnderlyingName": "int"
// CHECK:                                         },
// CHECK:                                         "c": {
// CHECK:                                                 "TypeName": "Test3",
// CHECK:                                                 "UnderlyingName": "int"
// CHECK:                                         },
// CHECK:                                         "d": {
// CHECK:                                                 "TypeName": "Test4",
// CHECK:                                                 "UnderlyingName": "uint64_t"
// CHECK:                                         },
// CHECK:                                         "e": {
// CHECK:                                                 "TypeName": "Test5",
// CHECK:                                                 "UnderlyingName": "int64_t"
// CHECK:                                         },
// CHECK:                                         "f": {
// CHECK:                                                 "TypeName": "Test6",
// CHECK:                                                 "UnderlyingName": "uint"
// CHECK:                                         },
// CHECK:                                         "g": {
// CHECK:                                                 "TypeName": "Test7",
// CHECK:                                                 "UnderlyingName": "int16_t"
// CHECK:                                         },
// CHECK:                                         "h": {
// CHECK:                                                 "TypeName": "Test8",
// CHECK:                                                 "UnderlyingName": "uint16_t"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
