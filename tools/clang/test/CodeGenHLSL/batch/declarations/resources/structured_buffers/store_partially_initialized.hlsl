// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

// Tests the printed layout of structured buffers.

struct Struct
{
    int4 v1;
    int4 v2;
};

RWStructuredBuffer<Struct> buf;

void main()
{
  Struct s;
  s.v1.yz = 0;
  buf[0] = s;
}