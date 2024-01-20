// RUN: %dxc -E main -T cs_6_6 %s | FileCheck %s

// CHECK: @main, !"main", null, null, [[PROPS:![0-9]+]]}
// CHECK: [[PROPS]] = !{i32 4, [[NT:![0-9]+]], i32 11, [[WS:![0-9]+]]}
// CHECK: [[NT]] = !{i32 1, i32 1, i32 8}
// CHECK: [[WS]] = !{i32 32}

[wavesize(32)]
[numthreads(1,1,8)]
void main() {
}
