// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// CHECK: Invalid shader stage attribute combination

[shader("compute")]
[shader("vertex")]
[ numthreads( 64, 2, 2 ) ]
void CVMain() {
}

