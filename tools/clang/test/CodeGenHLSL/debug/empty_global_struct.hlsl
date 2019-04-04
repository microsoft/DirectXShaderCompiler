// RUN: %dxc -E main -T ps_6_0 %s -Zi -Od | FileCheck %s

// CHECK-DAG: !DIGlobalVariable(name: "foo.0", linkageName: "foo.0", scope: !{{[0-9]+}}, file: !{{[0-9]+}}, line: {{[0-9]+}}, type: ![[ty:[0-9]+]]
// CHECK-DAG: ![[ty]] = !DIDerivedType(tag: DW_TAG_member, name: "S.0", file: !1, line: 6, baseType: !{{[0-9]+}}, size: 8

struct S {
  float f() {
    return 420;
  }
};

static S foo;

float main() : SV_Target {
  return foo.f();
}
