// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

[shader("compute")] 
[shader("compute")] 
[shader("node")] 
[nodedispatchgrid(4,1,1)]
[ numthreads( 64, 2, 2 ) ]  /* expected-no-diagnostics */
void CCNMain() {
}

[shader("compute")] 
[shader("compute")] 
[ numthreads( 64, 2, 2 ) ]  /* expected-no-diagnostics */
void CCMain() {
}

[shader("node")] 
[shader("node")] 
[nodedispatchgrid(8,1,1)]
[ numthreads( 64, 2, 2 ) ]  /* expected-no-diagnostics */
void NNMain() {
}

[shader("compute")]
[shader("node")]
[ numthreads( 64, 2, 2 ) ] /* expected-no-diagnostics */
[nodedispatchgrid(16,1,1)]
void CNMain() {
}


[shader("compute")] 
[shader("node")] 
[shader("compute")] 
[shader("node")] 
[nodedispatchgrid(32,1,1)]
[ numthreads( 64, 2, 2 ) ]  /* expected-no-diagnostics */
void CNCNMain() {
}
