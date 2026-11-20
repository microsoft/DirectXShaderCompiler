// RUN: not %dxc -T cs_6_0 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

// CHECK: error: parameter of type string is not supported
// CHECK-NEXT: void log(string text) {
void log(string text) {
  printf(text);
}

[numthreads(1, 1, 1)]
void main() {
  log("hello");
}
