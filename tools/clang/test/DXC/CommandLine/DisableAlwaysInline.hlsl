// RUN: %dxc -T ps_6_0 -fdisable-always-inline %s 2>&1 | FileCheck %s

// CHECK: warning: -fdisable-always-inline is unsupported in ps_6_0 profile, only supported in library shaders [-Winvalid-command-line-argument]
void main() {}
