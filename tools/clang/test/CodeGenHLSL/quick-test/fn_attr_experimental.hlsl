// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: define void
// CHECK: fn1
// @"\01?fn1@@YA?AV?$vector@M$03@@V1@@Z"
// CHECK: #0
// CHECK: attributes #0
// CHECK: "exp-foo"="bar"
// CHECK: "exp-zzz" "

[experimental("foo", "bar")]
[experimental("zzz", "")]
float4 fn1(float4 Input){
  return Input * 5.0f;
}
