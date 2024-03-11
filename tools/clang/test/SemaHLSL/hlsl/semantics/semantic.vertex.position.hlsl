// RUN: %dxc -T vs_6_0 -E main -spirv -verify %s

float4 main(float4 input : SV_Position) : POSITION { /* expected-warning{{'POSITION' is not considered equal to SV_Position but is treated as a user-defined semantic attribute}} */
  return input;
}
