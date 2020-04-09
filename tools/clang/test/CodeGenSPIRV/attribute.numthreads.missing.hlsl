// Run: %dxc -T cs_6_0 -E main

// CHECK: 4:6: error: thread group size [numthreads(x,y,z)] is missing from the entry-point function
void main() {}
