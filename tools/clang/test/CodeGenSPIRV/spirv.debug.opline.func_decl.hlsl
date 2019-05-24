// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.func_decl.hlsl

void foo();
void bar();

// CHECK:      OpLine [[file]] 24 1
// CHECK-NEXT: %main = OpFunction %void None
// CHECK:      OpLine [[file]] 14 1
// CHECK-NEXT: %foo = OpFunction %void None
void foo() {
  bar();
}

// CHECK:      OpLine [[file]] 20 1
// CHECK-NEXT: %bar = OpFunction %void None
void bar() {
  int a = 3;
}

void main() {
  foo();
  bar();
}
