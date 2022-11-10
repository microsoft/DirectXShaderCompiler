// RUN: %clang_cc1 -HV 2021 -fsyntax-only -ffreestanding -verify %s

template <typename T> void doSomething(uint pos) {
  globallycoherent RWTexture2D<T> output;
  globallycoherent Buffer<T> nonUAV; // expected-error {{'globallycoherent' is not a valid modifier for a non-UAV type}}
  globallycoherent T ThisShouldBreak = 2.0; // expected-error {{'globallycoherent' is not a valid modifier for a non-UAV type}}
  output[uint2(pos, pos)] = 0;
}

void doSomething2(uint pos) {
  globallycoherent RWTexture2D<float> output;
  globallycoherent float ThisShouldBreak = 2.0; // expected-error {{'globallycoherent' is not a valid modifier for a non-UAV type}}
}

[numthreads(8, 8, 1)] void main(uint threadId
                                : SV_DispatchThreadID) {
  doSomething<float>(threadId); // expected-note {{in instantiation of function template specialization 'doSomething<float>' requested here}}
  doSomething2(threadId);
}
