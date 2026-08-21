// RUN: %dxreflector %s | FileCheck %s

groupshared uint groupData[16];
static const uint staticData[4] = { 0, 1, 2, 3 };

struct Test {
	static const uint staticData = 0;
};

namespace tst {

	static const uint staticData[4] = { 0, 1, 2, 3 };

	struct Test {
		static const uint staticData = 0;
	};
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
// CHECK:                         "Name": "groupData",
// CHECK:                         "NodeType": "GroupsharedVariable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "uint",
// CHECK:                                 "ArraySize": [
// CHECK:                                         16
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "staticData",
// CHECK:                         "NodeType": "StaticVariable",
// CHECK:                         "Type": {
// CHECK:                                 "Name": "uint",
// CHECK:                                 "ArraySize": [
// CHECK:                                         4
// CHECK:                                 ]
// CHECK:                         }
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "Test",
// CHECK:                         "NodeType": "Struct",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "staticData",
// CHECK:                                         "NodeType": "StaticVariable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "uint"
// CHECK:                                         }
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 },
// CHECK:                 {
// CHECK:                         "Name": "tst",
// CHECK:                         "NodeType": "Namespace",
// CHECK:                         "Children": [
// CHECK:                                 {
// CHECK:                                         "Name": "staticData",
// CHECK:                                         "NodeType": "StaticVariable",
// CHECK:                                         "Type": {
// CHECK:                                                 "Name": "uint",
// CHECK:                                                 "ArraySize": [
// CHECK:                                                         4
// CHECK:                                                 ]
// CHECK:                                         }
// CHECK:                                 },
// CHECK:                                 {
// CHECK:                                         "Name": "Test",
// CHECK:                                         "NodeType": "Struct",
// CHECK:                                         "Children": [
// CHECK:                                                 {
// CHECK:                                                         "Name": "staticData",
// CHECK:                                                         "NodeType": "StaticVariable",
// CHECK:                                                         "Type": {
// CHECK:                                                                 "Name": "uint"
// CHECK:                                                         }
// CHECK:                                                 }
// CHECK:                                         ]
// CHECK:                                 }
// CHECK:                         ]
// CHECK:                 }
// CHECK:         ]
// CHECK: }
