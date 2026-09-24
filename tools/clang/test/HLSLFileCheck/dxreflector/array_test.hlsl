// RUN: %dxreflector %s | FileCheck %s

typedef float farr4[4];
typedef float farr4x3[4][3];
typedef float farr4x3x5[4][3][5];

typedef farr4 farr4a6[6];
typedef farr4x3 farr4x3a6a7[6][7];
typedef farr4x3x5 farr4x3x5a6a7a8[6][7][8];

typedef float f32;

typedef f32 f32arr4[4];
typedef f32 f32arr4x3[4][3];
typedef f32 f32arr4x3x5[4][3][5];

typedef f32arr4 f32arr4a6[6];
typedef f32arr4x3 f32arr4x3a6a7[6][7];
typedef f32arr4x3x5 f32arr4x3x5a6a7a8[6][7][8];

farr4 arr0[2];
farr4x3 arr1[2];
farr4x3x5 arr2[2];

farr4a6 arr3[2];
farr4x3a6a7 arr4[2];
farr4x3x5a6a7a8 arr5[2];

f32arr4 arr6[2];
f32arr4x3 arr7[2];
f32arr4x3x5 arr8[2];

f32arr4a6 arr9[2];
f32arr4x3a6a7 arr10[2];
f32arr4x3x5a6a7a8 arr11[2];

struct Test {

    farr4 arr0[2];
    farr4x3 arr1[2];
    farr4x3x5 arr2[2];

    farr4a6 arr3[2];
    farr4x3a6a7 arr4[2];
    farr4x3x5a6a7a8 arr5[2];

    f32arr4 arr6[2];
    f32arr4x3 arr7[2];
    f32arr4x3x5 arr8[2];

    f32arr4a6 arr9[2];
    f32arr4x3a6a7 arr10[2];
    f32arr4x3x5a6a7a8 arr11[2];
};

void func(
    Test t[2], 
    farr4 arr0[2],
    farr4x3 arr1[2],
    farr4x3x5 arr2[2],
    farr4a6 arr3[2],
    farr4x3a6a7 arr4[2],
    farr4x3x5a6a7a8 arr5[2],
    f32arr4 arr6[2],
    f32arr4x3 arr7[2],
    f32arr4x3x5 arr8[2],
    f32arr4a6 arr9[2],
    f32arr4x3a6a7 arr10[2],
    f32arr4x3x5a6a7a8 arr11[2]) { }

Texture2D tex0[4];
Texture2D tex1[4][3];
Texture2D tex2[4][3][2];

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
// CHECK:                         "Name": "farr4",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "farr4x3",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4,
// CHECK:                                         3
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "farr4x3x5",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "farr4a6",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4",
// CHECK:                                 "ArraySize": [
// CHECK:                                         6
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         6
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "farr4x3a6a7",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4x3",
// CHECK:                                 "ArraySize": [
// CHECK:                                         6,
// CHECK:                                         7
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         6,
// CHECK:                                         7
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "farr4x3x5a6a7a8",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4x3x5",
// CHECK:                                 "ArraySize": [
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         8
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5,
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         8
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32arr4",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32arr4x3",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4,
// CHECK:                                         3
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32arr4x3x5",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float"
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32arr4a6",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4",
// CHECK:                                 "ArraySize": [
// CHECK:                                         6
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         6
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32arr4x3a6a7",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4x3",
// CHECK:                                 "ArraySize": [
// CHECK:                                         6,
// CHECK:                                         7
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         6,
// CHECK:                                         7
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "f32arr4x3x5a6a7a8",
// CHECK:                         "NodeType": "Typedef",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4x3x5",
// CHECK:                                 "ArraySize": [
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         8
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5,
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         8
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr0",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr1",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4x3",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr2",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4x3x5",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr3",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4a6",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         6,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr4",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4x3a6a7",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr5",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "farr4x3x5a6a7a8",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5,
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         8,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr6",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr7",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4x3",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr8",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4x3x5",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr9",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4a6",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         6,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr10",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4x3a6a7",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "arr11",
// CHECK:                         "NodeType": "Variable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "f32arr4x3x5a6a7a8",
// CHECK:                                 "ArraySize": [
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "UnderlyingName": "float",
// CHECK:                                 "UnderlyingArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         5,
// CHECK:                                         6,
// CHECK:                                         7,
// CHECK:                                         8,
// CHECK:                                         2
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "arr0",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "farr4",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr1",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "farr4x3",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr2",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "farr4x3x5",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr3",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "farr4a6",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         6,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr4",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "farr4x3a6a7",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr5",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "farr4x3x5a6a7a8",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         8,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr6",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32arr4",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr7",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32arr4x3",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr8",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32arr4x3x5",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr9",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32arr4a6",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         6,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr10",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32arr4x3a6a7",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "arr11",
// CHECK:                                         "NodeType": "Variable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "f32arr4x3x5a6a7a8",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         8,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "func",
// CHECK:                         "NodeType": "Function",
// CHECK:                         "Function": {
// CHECK:                                 "Params": {
// CHECK:                                         "t": {
// CHECK:                                                 "TypeName": "Test",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr0": {
// CHECK:                                                 "TypeName": "farr4",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr1": {
// CHECK:                                                 "TypeName": "farr4x3",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr2": {
// CHECK:                                                 "TypeName": "farr4x3x5",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr3": {
// CHECK:                                                 "TypeName": "farr4a6",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         6,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr4": {
// CHECK:                                                 "TypeName": "farr4x3a6a7",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr5": {
// CHECK:                                                 "TypeName": "farr4x3x5a6a7a8",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         8,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr6": {
// CHECK:                                                 "TypeName": "f32arr4",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr7": {
// CHECK:                                                 "TypeName": "f32arr4x3",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr8": {
// CHECK:                                                 "TypeName": "f32arr4x3x5",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr9": {
// CHECK:                                                 "TypeName": "f32arr4a6",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         6,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr10": {
// CHECK:                                                 "TypeName": "f32arr4x3a6a7",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         },
// CHECK:                                         "arr11": {
// CHECK:                                                 "TypeName": "f32arr4x3x5a6a7a8",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         2
// CHECK:                                                 ],
// CHECK:                                                 "UnderlyingName": "float",
// CHECK:                                                 "UnderlyingArraySize": [
// CHECK:                                                         4,
// CHECK:                                                         3,
// CHECK:                                                         5,
// CHECK:                                                         6,
// CHECK:                                                         7,
// CHECK:                                                         8,
// CHECK:                                                         2
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 "ReturnType": {
// CHECK:                                         "TypeName": "void"
// CHECK:                                 }
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "tex0",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "BindCount": 4,
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "tex1",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "BindCount": 12,
// CHECK:                                 "ArraySize": [
// CHECK:                                         4,
// CHECK:                                         3
// CHECK:                                 ],
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "tex2",
// CHECK:                         "NodeType": "Register",
// CHECK:                         "Register": {
// CHECK:                                 "RegisterType": "Texture",
// CHECK:                                 "Dimension": "Texture2D",
// CHECK:                                 "ReturnType": "float",
// CHECK:                                 "BindCount": 24,
// CHECK:                                 "ArraySize": [
// CHECK:                                         4,
// CHECK:                                         3,
// CHECK:                                         2
// CHECK:                                 ],
// CHECK:                                 "Flags": [
// CHECK:                                         "TextureComponent0",
// CHECK:                                         "TextureComponent1"
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 }
// CHECK:         ]
// CHECK: }
