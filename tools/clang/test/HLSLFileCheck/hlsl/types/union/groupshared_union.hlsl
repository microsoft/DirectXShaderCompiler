// RUN: %dxc -E main -HV 202x -T cs_6_0 %s | FileCheck %s

union Foo {
    int a;
    int d;
};

RWBuffer<Foo> inputs;
RWBuffer<int> outputs;

// CHECK: error: union objects cannot be in groupshared memory
groupshared Foo sharedData;

[numthreads(1, 1, 1)]
void main(uint GI : SV_GroupIndex)
{
  if (GI == 0) {
    sharedData = inputs[0];
  }
  outputs[GI] = sharedData.d;
}
