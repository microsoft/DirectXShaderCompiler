// RUN: %dxc -T lib_6_5 %s | FileCheck %s

// CHECK: Invalid shader stage attribute combination

[shader("mesh")]
[shader("pixel")]
void MPMain() {
}

