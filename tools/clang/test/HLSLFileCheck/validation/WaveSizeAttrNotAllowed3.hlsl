// RUN: %dxc -T cs_6_6 -E main %s | FileCheck %s

// this test should pass, because despite the fact that 
// main2 isn't the current entry point function, 
// the fact that it's callgraph-reachable from the entry
// point function "main" gives it permission to have
// "wavesize" as an "entry point attribute"

// CHECK: @main, !"main", null, null, [[PROPS:![0-9]+]]}
// CHECK: [[PROPS]] = !{i32 4, [[NT:![0-9]+]]}
// CHECK: [[NT]] = !{i32 2, i32 2, i32 1}

[wavesize(32)]
[numthreads(2,2,1)]
void main2() {}

[numthreads(2,2,1)]
void main() {main2();}