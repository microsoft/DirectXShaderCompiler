// RUN: %dxc /T ms_6_6 /E MSMain %s | FileCheck %s
// CHECK: Mesh Shader
// CHECK: Numthreads: (2,3,1)


[NumThreads(2,3,1)]
[OutputTopology("triangle")] 
void  MSMain() {
  int x = 2;
}
