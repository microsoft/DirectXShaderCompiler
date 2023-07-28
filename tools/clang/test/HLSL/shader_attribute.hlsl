// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

[shader("compute")]/* expected-note {{conflicting attribute is here}} */
[shader("vertex")]  /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[ numthreads( 64, 2, 2 ) ] 

void CVMain() {
}

[shader("vertex")] /* expected-note {{conflicting attribute is here}} */
[shader("pixel")] /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[ numthreads( 64, 2, 2 ) ] 
void VGMain() {
}

[shader("vertex")] /* expected-note {{conflicting attribute is here}} */
[shader("node")] /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[ numthreads( 64, 2, 2 ) ] 
void VNMain() {
}

[shader("compute")] /* expected-note {{conflicting attribute is here}} */
[shader("vertex")] /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[shader("node")] /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[ numthreads( 64, 2, 2 ) ] 
void CVNMain() {
}

[shader("mesh")] /* expected-note {{conflicting attribute is here}} */
[shader("pixel")]  /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[ numthreads( 64, 2, 2 ) ]
void MPMain() {
}

[shader("compute")] /* expected-note {{conflicting attribute is here}} */
[shader("vertex")]  /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[shader("compute")] /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[ numthreads( 64, 2, 2 ) ]
void CVCMain() {
}

[shader("node")]   /* expected-note {{conflicting attribute is here}} */ 
[shader("vertex")] /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[shader("pixel")]  /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[ numthreads( 64, 2, 2 ) ]
void NVPMain() {
}