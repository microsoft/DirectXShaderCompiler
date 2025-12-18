// RUN: %dxc -T cs_6_3 -enable-16bit-types -HV 202x -verify %s

RWStructuredBuffer<uint4> Out : register(u0);

groupshared uint16_t SharedData;

void fn1(groupshared half Sh) {
// expected-note@-1{{candidate function not viable: 1st argument ('half') is in address space 0, but parameter must be in address space}}
  Sh = 5;
}

[numthreads(4, 1, 1)]
void main(uint3 TID : SV_GroupThreadID) {
  half tmp = 1.0;
  fn1(tmp);
  // expected-error@-1{{no matching function for call to 'fn1'}}
  Out[TID.x] = (uint4) SharedData.xxxx;
}
