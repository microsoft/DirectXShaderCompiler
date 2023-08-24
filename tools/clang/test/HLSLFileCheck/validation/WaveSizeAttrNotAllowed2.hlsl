// RUN: %dxc -T lib_6_7 %s | FileCheck %s

// CHECK: error: attribute WaveSize requires shader stage compute
[WaveSize(64)]
void N()
{
    return;
}