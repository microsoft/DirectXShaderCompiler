// RUN: %dxc -E S -T cs_6_0 %s | FileCheck %s

// CHECK: error: attribute wavesize must be in at least shader model 6.6
[WaveSize(64)]
[numthreads(2,2,4)]
void S()
{
    return;
}
