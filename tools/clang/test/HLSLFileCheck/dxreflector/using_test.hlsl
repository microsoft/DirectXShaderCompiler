// RUN: %dxreflector %s | FileCheck %s

using F32 = float;
using F64 = double;

typedef float F32x;
typedef double F64x;

using F32a = F32;
typedef F32 F32a2;

struct Test {
    F32 a;
    F64 b;
    F32x c;
    F64x d;
    F32a e;
    F32a2 f;
};

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
// CHECK:                         "Name": "F32",
// CHECK:                         "NodeType": "Using",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "F64",
// CHECK:                         "NodeType": "Using",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "double"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "F32x",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "F64x",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "double"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "F32a",
// CHECK:                         "NodeType": "Using",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "F32",
// CHECK:                                 "UnderlyingName": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "F32a2",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "F32",
// CHECK:                                 "UnderlyingName": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "a",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "F32",
// CHECK:                                                 "UnderlyingName": "float"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "b",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "F64",
// CHECK:                                                 "UnderlyingName": "double"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "c",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "F32x",
// CHECK:                                                 "UnderlyingName": "float"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "d",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "F64x",
// CHECK:                                                 "UnderlyingName": "double"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "e",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "F32a",
// CHECK:                                                 "UnderlyingName": "F32"
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "f",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "F32a2",
// CHECK:                                                 "UnderlyingName": "F32"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ]
// CHECK: }
