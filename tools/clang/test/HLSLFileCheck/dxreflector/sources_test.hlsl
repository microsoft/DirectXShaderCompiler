// RUN: %dxreflector -reflect-show-file-info %s | FileCheck %s

typedef float B;
float a;

enum class Test {
	A,
	B
};

struct A {
	float a;
	void test() {}
};

void test(A a, float b) {}

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
// CHECK:         "Children": [
// CHECK:                 {
// CHECK:                         "Name": "B",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Symbol": {
// CHECK:                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                 "LineId": 3,
// CHECK:                                 "ColumnStart": 1,
// CHECK:                                 "ColumnEnd": 16
// CHECK:                         },
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "a",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Symbol": {
// CHECK:                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                 "LineId": 4,
// CHECK:                                 "ColumnStart": 1,
// CHECK:                                 "ColumnEnd": 8
// CHECK:                         },
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test",
// CHECK:                         "NodeType": "Enum",
// CHECK:                         "Symbol": {
// CHECK:                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                 "LineId": 6,
// CHECK:                                 "LineCount": 4,
// CHECK:                                 "ColumnStart": 1,
// CHECK:                                 "ColumnEnd": 2
// CHECK:                         },
// CHECK:                         "Enum": {
// CHECK:                                 "Name": "Test",
// CHECK:                                 "EnumType": "int",
// CHECK:                                 "Values": [
// CHECK:                                         {
// CHECK:                                                 "Value": 0,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "A",
// CHECK:                                                         "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                                         "LineId": 7,
// CHECK:                                                         "ColumnStart": 2,
// CHECK:                                                         "ColumnEnd": 3
// CHECK:                                                 }
// CHECK:                                         },
// CHECK:                                         {
// CHECK:                                                 "Value": 1,
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Name": "B",
// CHECK:                                                         "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                                         "LineId": 8,
// CHECK:                                                         "ColumnStart": 2,
// CHECK:                                                         "ColumnEnd": 3
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "A",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Symbol": {
// CHECK:                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                 "LineId": 11,
// CHECK:                                 "LineCount": 4,
// CHECK:                                 "ColumnStart": 1,
// CHECK:                                 "ColumnEnd": 2
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Symbol": {
// CHECK:                                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                                 "LineId": 12,
// CHECK:                                                 "ColumnStart": 2,
// CHECK:                                                 "ColumnEnd": 9
// CHECK:                                         },
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "test",
// CHECK:                                         "NodeType": "Function",
// CHECK:                                         "Symbol": {
// CHECK:                                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                                 "LineId": 13,
// CHECK:                                                 "ColumnStart": 2,
// CHECK:                                                 "ColumnEnd": 16
// CHECK:                                         },
// CHECK:                                         "Function": {
// CHECK:                                                 "Params": {
// CHECK:                                                 },
// CHECK:                                                 "ReturnType": {
// CHECK:                                                         "TypeName": "void"
// CHECK:                                                 }
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "test",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Symbol": {
// CHECK:                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                 "LineId": 16,
// CHECK:                                 "ColumnStart": 1,
// CHECK:                                 "ColumnEnd": 27
// CHECK:                         },
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "a": {
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                                         "LineId": 16,
// CHECK:                                                         "ColumnStart": 11,
// CHECK:                                                         "ColumnEnd": 14
// CHECK:                                                 },
// CHECK:                                                 "TypeName": "A"
// CHECK:                                         },
// CHECK:                                         "b": {
// CHECK:                                                 "Symbol": {
// CHECK:                                                         "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                                         "LineId": 16,
// CHECK:                                                         "ColumnStart": 16,
// CHECK:                                                         "ColumnEnd": 23
// CHECK:                                                 },
// CHECK:                                                 "TypeName": "float"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "tst",
// CHECK:                         "NodeType": "Namespace",
// CHECK:                         "Symbol": {
// CHECK:                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                 "LineId": 18,
// CHECK:                                 "LineCount": 3,
// CHECK:                                 "ColumnStart": 1,
// CHECK:                                 "ColumnEnd": 2
// CHECK:                         },
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Symbol": {
// CHECK:                                                 "Source": "{{.*}}sources_test.hlsl",
// CHECK:                                                 "LineId": 19,
// CHECK:                                                 "ColumnStart": 2,
// CHECK:                                                 "ColumnEnd": 9
// CHECK:                                         },
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "float"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ]
// CHECK: }
