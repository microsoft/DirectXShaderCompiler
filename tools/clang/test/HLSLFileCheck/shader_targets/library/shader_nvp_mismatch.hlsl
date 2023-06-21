// RUN: %dxc -T lib_6_8 %s | FileCheck %s

// CHECK: Invalid shader stage attribute combination

[shader("node")]
[shader("vertex")]
[shader("pixel")]
void NVPMain() {
}

