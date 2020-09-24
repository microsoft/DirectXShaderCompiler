
RWStructuredBuffer<uint> output : register( u256 );
cbuffer minimal_cbuffer : register(b256) {
  float a,b,c,d,e,f,g,h;
  uint t;
  uint count;
}

bool foo() {
  if (e > a)
    return true;
  if (b > 0) {
    if (d >= h)
      return true;
    if (f >= c)
      return true;
  }
  return false;
}

[numthreads(64, 1, 1)]
void main(uint i : SV_GroupIndex) {
  uint idx = 0;
  uint mask = 0;
  if (!foo())
    mask |= uint((t >> 24) & 0x7);
  if (!(mask & 4))
    idx |= 1;
  output[idx] += 1;
}



