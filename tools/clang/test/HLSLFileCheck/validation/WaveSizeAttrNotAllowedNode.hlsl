// RUN: %dxc -T lib_6_7 %s | FileCheck %s
// note, this test would pass if the node shader
// launch type was NodeLaunchType::Thread


// CHECK: error: Node shader 'N' with broadcasting launch type requires 'numthreads' attribute
[WaveSize(64)]
[shader("node")]
void N()
{
    return;
}
