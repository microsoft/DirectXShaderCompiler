// RUN: %dxc -E main -T ps_6_0 %s -Zi -Od | FileCheck %s

// CHECK-DAG: ![[S:.*]] = !DICompositeType(tag: DW_TAG_structure_type, name: "S", {{.*}}, align: 8,
// CHECK-DAG: !DIGlobalVariable(name: "foo", {{.*}}, type: ![[S]], isLocal: true, isDefinition: true)

struct S {
  float f() {
    return 420;
  }
};

static S foo;

float main() : SV_Target {
  return foo.f();
}
