// RUN: not %dxc -T cs_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s --implicit-check-not="fatal error:"
// RUN: not %dxc -T cs_6_0 -E main -fcgl -DLOCAL_STRING %s -spirv 2>&1 | FileCheck %s --implicit-check-not="fatal error:"

// CHECK: error: Found uninitialized string variable.
#ifndef LOCAL_STRING
string first;
#endif

[numthreads(1,1,1)]
void main() {
#ifdef LOCAL_STRING
  string local;
  printf(local);
#endif
}

