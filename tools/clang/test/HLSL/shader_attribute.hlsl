// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

[shader("compute")]/* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[shader("vertex")]  /* expected-note {{conflicting attribute is here}} */ 
[ numthreads( 64, 2, 2 ) ] 

void CVMain() {
}

[shader("vertex")] /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */ 
[shader("pixel")] /* expected-note {{conflicting attribute is here}} */ 
[ numthreads( 64, 2, 2 ) ] 
void VGMain() {
}

[shader("vertex")] /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[shader("node")] /* expected-note {{conflicting attribute is here}} */ 
[ numthreads( 64, 2, 2 ) ] 
void VNMain() {
}

[shader("compute")] 
[shader("vertex")]  /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[shader("node")] /* expected-note {{conflicting attribute is here}} */ 
[ numthreads( 64, 2, 2 ) ] 
void CVNMain() {
}

[shader("mesh")] /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[shader("pixel")]  /* expected-note {{conflicting attribute is here}} */
[ numthreads( 64, 2, 2 ) ]
void MPMain() {
}

[shader("compute")] 
[shader("vertex")]  /* expected-error {{invalid shader stage attribute combination}} */  /* expected-note {{conflicting attribute is here}} */
[shader("compute")]  /* expected-note {{conflicting attribute is here}} */
[ numthreads( 64, 2, 2 ) ]
void CVCMain() {
}

[shader("node")]   /* expected-note {{conflicting attribute is here}} */ /* expected-error {{invalid shader stage attribute combination}} */
[shader("vertex")] /* expected-error {{invalid shader stage attribute combination}} */ /* expected-note {{conflicting attribute is here}} */
[shader("pixel")]  /* expected-note {{conflicting attribute is here}} */  /* expected-note {{conflicting attribute is here}} */ 
void NVPMain() {
}

[shader("I'm invalid")]   /* expected-error {{attribute 'shader' must have one of these values: compute,vertex,pixel,hull,domain,geometry,raygeneration,intersection,anyhit,closesthit,miss,callable,mesh,amplification,node}} */
void InvalidMain() {
}