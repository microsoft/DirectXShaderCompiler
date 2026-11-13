// RUN: %dxc -E main -T as_6_5 %s | FileCheck %s
// CHECK: Amplification Shader
// CHECK: dx.op.dispatchMesh
struct Payload { float value; };
[numthreads(1,1,1)] void main() {
 Payload p; p.value = 7;
 DispatchMesh(1,1,1,p);
}
