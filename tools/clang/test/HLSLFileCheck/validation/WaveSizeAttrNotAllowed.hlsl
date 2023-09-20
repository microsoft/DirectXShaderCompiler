// RUN: %dxc -T lib_6_7 %s | FileCheck %s

// CHECK: error: attribute numthreads requires shader stage compute, mesh, or amplification
[WaveSize(64)]
[numthreads(2,2,4)]
void S()
{
    return;
}
