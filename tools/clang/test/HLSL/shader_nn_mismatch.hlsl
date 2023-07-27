// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s


[shader("node")] 
[shader("node")] 
[ numthreads( 64, 2, 2 ) ]  /* expected-no-diagnostics */
void CVNMain() {
}