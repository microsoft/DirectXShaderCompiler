// RUN: %dxc -T lib_6_7 %s | FileCheck %s

// CHECK-NOT: error:
// CHECK-NOT: warning:

[WaveSize(64)]
[numthreads(2,2,4)]
void S()
{
    return;
}
