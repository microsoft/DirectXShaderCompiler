// Run: %dxc -T cs_6_0 -E main

// CHECK: 4:8: error: Found uninitialized string variable.
string first;

[numthreads(1,1,1)]
void main() {}

