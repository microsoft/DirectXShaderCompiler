// RUN: %dxc -T cs_6_6 -E main -dxil-dll-path D:\DXC\dxc_releases\1.7.2212.github_release\x64\dxil.dll 2>&1 %s | FileCheck %s

[shader("compute")]
[numthreads(2,2,1)]
// CHECK: warning: External validator loaded at D:\DXC\dxc_releases\1.7.2212.github_release\x64\dxil.dll
void main() {}
