RWByteAddressBuffer output1 : register(u2);
RWByteAddressBuffer output2 : register(u15);
RWByteAddressBuffer output3 : register(u0);
RWByteAddressBuffer output4 : register(u0, space1);
RWByteAddressBuffer output5 : SEMA : register(u16);
RWByteAddressBuffer output6 : register(u17);
RWByteAddressBuffer output7 : register(u1);
RWByteAddressBuffer output8[12] : register(u3);
RWByteAddressBuffer output9[12] : register(u18);
RWByteAddressBuffer output10[33] : register(u1, space1);
RWByteAddressBuffer output11[33] : register(u33, space2);
RWByteAddressBuffer output12[33] : register(u0, space2);
StructuredBuffer<float> test : register(t1);
ByteAddressBuffer input13 : SEMA : register(t2);
ByteAddressBuffer input14 : register(t15);
ByteAddressBuffer input15 : register(t0);
ByteAddressBuffer input16[12] : register(t3);
ByteAddressBuffer input17[2] : register(t13, space1);
ByteAddressBuffer input18[12] : register(t1, space1);
ByteAddressBuffer input19[3] : register(t15, space1);
ByteAddressBuffer input20 : register(t0, space1);
SamplerState sampler0 : register(s1);
SamplerState sampler1 : register(s2);
SamplerState sampler2 : register(s0);
SamplerState sampler3 : register(s1, space1);
SamplerState sampler4 : register(s0, space1);
cbuffer test : register(b0) {
  const float a;
}
;
cbuffer test2 : register(b1) {
  const float b;
}
;
cbuffer test3 : register(b0, space1) {
  const float c;
}
;
cbuffer test4 : register(b1, space1) {
  const float d;
}
;
const float e;
[numthreads(16, 16, 1)]
void main(uint id : SV_DispatchThreadID) {
  output2.Store(id * 4, 1);
}


