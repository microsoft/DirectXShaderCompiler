// RUN: %dxc -T cs_6_0 -E main

RWStructuredBuffer<uint> Out;
groupshared uint Mem[1];
[numthreads(1, 1, 1)]
void main() {
  // CHECK: [[sub:%\d+]] = OpISub %int %int_1 %int_1
  // CHECK:     {{%\d+}} = OpAccessChain %_ptr_Workgroup_uint %Mem [[sub]]
  Mem[1 - 1] = 0;
  Out[0] = Mem[0];
}

