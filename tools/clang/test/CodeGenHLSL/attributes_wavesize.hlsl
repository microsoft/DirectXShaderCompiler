// RUN: %dxc -E main -T cs_6_6 %s -D WAVESIZE=13 | FileCheck %s -check-prefixes=CHECK-ERR

// CHECK: @main, !"main", null, null, [[PROPS:![0-9]+]]}
// CHECK: [[PROPS]] = !{i32 4, [[NT:![0-9]+]], i32 11, [[WS:![0-9]+]]}
// CHECK: [[NT]] = !{i32 1, i32 1, i32 8}
// CHECK: [[WS]] = !{i32 32}

// CHECK-ERR: error: WaveSize arguments must be between 4 and 128 and a power of 2

#ifndef WAVESIZE
#define WAVESIZE 32
#endif

[wavesize(WAVESIZE)]
[numthreads(1,1,8)]
void main() {
}