// RUN: %dxc -E main -T lib_6_6 %s -verify

[wavesize(4)]
[numthreads(1,1,8)]
[shader("compute")]
void one() {
}

[wavesize(4, 8)] // expected-error{{wavesize attribute takes no more than 1 argument}}
[numthreads(1,1,8)]
[shader("compute")]
void two() {
}

[wavesize(4,8, 8)] // expected-error{{wavesize attribute takes no more than 1 argument}}
[numthreads(1,1,8)]
[shader("compute")]
void three() {
}
