// RUN: %dxc -T vs_6_0 -E main -Od -Zi %s | FileCheck %s

// Test that the debug info for HLSL vectors exposes per-component fields.

// CHECK: !DIDerivedType(tag: DW_TAG_typedef, name: "int2"
// CHECK: !DICompositeType(tag: DW_TAG_class_type, name: "vector<int, 2>", {{.*}}, size: 64, align: 32
// CHECK: !DIDerivedType(tag: DW_TAG_member, name: "x", {{.*}}, size: 32, align: 32
// CHECK: !DIDerivedType(tag: DW_TAG_member, name: "y", {{.*}}, size: 32, align: 32, offset: 32
int2 main(int2 v : IN) : OUT { return v; }