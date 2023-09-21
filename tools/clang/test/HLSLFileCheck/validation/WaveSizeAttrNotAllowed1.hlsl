// RUN: %dxc -E S -T cs_6_0 %s | FileCheck %s
// RUN: %dxc -T lib_6_7 %s | FileCheck %s -check-prefixes=CHECK-LIB

// CHECK: error: attribute wavesize requires shader model 6.6 or greater
// CHECK-LIB-NOT: error:
// CHECK-LIB-NOT: warning:

[WaveSize(64)]
[numthreads(2,2,4)]
void S()
{
    return;
}
