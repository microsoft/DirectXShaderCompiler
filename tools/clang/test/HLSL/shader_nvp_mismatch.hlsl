// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

[shader("node")]   /* expected-note {{conflicting attribute is here}} */ 
[shader("vertex")] /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[shader("pixel")]  /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[ numthreads( 64, 2, 2 ) ]
void NVPMain() {
}

