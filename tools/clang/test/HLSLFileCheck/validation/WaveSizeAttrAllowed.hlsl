// RUN: %dxc -T lib_6_7 %s | FileCheck %s

// This test used to fail because it didn't take into account
// the possibility of the compute shader being a library target.
// Now, this test is an example of what should be allowed
// and no errors are expected.

// CHECK: @CS, !"CS", null, null, [[PROPS:![0-9]+]]}
// CHECK: [[PROPS]] = !{i32 8, i32 5, i32 4, [[NT:![0-9]+]], i32 11, [[WS:![0-9]+]]
// CHECK: [[NT]] = !{i32 2, i32 2, i32 1}
// CHECK: [[WS]] = !{i32 32}

[Shader("compute")]
[WaveSize(32)]
[numthreads(2,2,1)]
void CS()
{
    return;
}
