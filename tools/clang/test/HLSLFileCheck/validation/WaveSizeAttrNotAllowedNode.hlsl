// RUN: %dxc -T lib_6_7 %s | FileCheck %s
// note, this test would pass is the node shader 
// launch type was NodeLaunchType::Thread


// CHECK: error: NumThreads is required, but was not specified
[WaveSize(64)]
[shader("node")]
void N()
{
    return;
}
