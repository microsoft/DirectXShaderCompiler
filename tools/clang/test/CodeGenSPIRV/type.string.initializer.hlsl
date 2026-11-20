// RUN: not %dxc -T cs_6_0 -E main -fcgl %s -spirv 2>&1 | FileCheck %s --implicit-check-not="fatal error:"

string globalText = "text";

[numthreads(1, 1, 1)]
void main() {
  // CHECK: error: string variable initializer must be a string literal with -spirv
  // CHECK-NEXT: string local = globalText;
  string local = globalText;
  printf(local);
}
