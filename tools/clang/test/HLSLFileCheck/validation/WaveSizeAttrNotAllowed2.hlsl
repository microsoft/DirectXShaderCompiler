// RUN: %dxc -T lib_6_7 %s | FileCheck %s

// CHECK: error: attribute WaveSize only valid for CS.
[WaveSize(64)]
void N()
{
    return;
}