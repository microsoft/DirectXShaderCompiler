// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s


[shader("compute")] 
[shader("compute")] 
[ numthreads( 64, 2, 2 ) ]  /* expected-no-diagnostics */
void CVNMain() {
}