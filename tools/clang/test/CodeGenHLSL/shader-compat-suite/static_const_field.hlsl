// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure static const field works.

// CHECK: float 6.000000e+00

struct Tex2DAndSampler {

static const uint NumberOfReservedSamplers = 6 ;

};


float test() {
  return Tex2DAndSampler::NumberOfReservedSamplers ;
}