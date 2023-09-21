// RUN: %dxc -T cs_6_6 -E csmain1 %s | FileCheck %s -check-prefixes=CHECK-MAIN1
// RUN: %dxc -T cs_6_6 -E csmain2 %s | FileCheck %s -check-prefixes=CHECK-MAIN2

// another regression test that we should be sure compiles correctly

// CHECK-MAIN1: @csmain1, !"csmain1", null, null, [[PROPS:![0-9]+]]}
// CHECK-MAIN2: @csmain2, !"csmain2", null, null, [[PROPS:![0-9]+]]}

// CHECK: [[PROPS]] = !{i32 4, [[NT:![0-9]+]], i32 11, [[WS:![0-9]+]]}
// CHECK: [[NT]] = !{i32 32, i32 1, i32 1}
// CHECK: [[WS]] = !{i32 32}



[WaveSize(32)]
[numthreads(32, 1, 1)]
void csmain1(){ }

[WaveSize(32)]
[numthreads(32, 1, 1)]
void csmain2(){ }