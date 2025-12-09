// RUN: %dxc -T cs_6_6 -E main -spirv %s -enable-16bit-types -O0

//StructuredBuffer<int16_t> In : register(t0);
//RWStructuredBuffer<int16_t> Out : register(u2);

[numthreads(1,1,1)]
void main() {
  uint16_t u_16 = 1;
  uint32_t a = countbits(u_16);

  int16_t s_16 = 1;
  uint32_t b = countbits(s_16);

  uint64_t u_64 = 1;
  uint32_t c = countbits(u_64);

  int64_t s_64 = 1;
  uint32_t d = countbits(s_64);
  //Out[0] = countbits(In[0]);
}
