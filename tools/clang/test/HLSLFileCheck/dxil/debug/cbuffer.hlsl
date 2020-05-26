// RUN: %dxc -E main -T ps_6_0 %s -Od -Zi | FileCheck %s

cbuffer cb {
  float g_foo;
  float g_bar;
};

const float g_baz;
const float g_bee;


float main() : SV_Target {
  return 0;
}

// CHECK-DAG: !{{[0-9]+}} = !DIGlobalVariable(name: "g_foo"
// CHECK-DAG: !{{[0-9]+}} = !DIGlobalVariable(name: "g_bar"
// CHECK-DAG: !{{[0-9]+}} = !DIGlobalVariable(name: "g_baz"
// CHECK-DAG: !{{[0-9]+}} = !DIGlobalVariable(name: "g_bee"

