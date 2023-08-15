// RUN: %dxc -T lib_6_7 %s | FileCheck %s

// CHECK: error: attribute numthreads only valid for CS/MS/AS.
[WaveSize(64)]
[numthreads(2,2,4)]
void S()
{
    return;
}
