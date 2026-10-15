// RUN: %dxc -T cs_6_0 -Od -Zi %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,UAVSize=1048576 | %FileCheck %s -check-prefix=SIZE
// RUN: %dxc -T cs_6_0 -Od -Zi %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,UAVSize=1048576 | %FileCheck %s -check-prefix=NO-OLD

RWByteAddressBuffer RawUAV : register(u0);

[numthreads(1, 1, 1)] void main() {
  double local_array[4];
  uint index = RawUAV.Load(0);
  double value = asdouble(RawUAV.Load(4), RawUAV.Load(8));
  if (index < 4) {
    local_array[index] = value;
  }
  RawUAV.Store(12, asuint((float)local_array[0]));
}

// The store block reserves a 12-byte header and a 4-byte index payload.
// SIZE-LABEL: define void @main()
// SIZE: getelementptr inbounds [4 x double], [4 x double]* %{{[a-zA-Z0-9._]+}}, i32 0, i32 %{{[a-zA-Z0-9._]+}}
// SIZE: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle {{.*}}, i32 0, i32 {{.*}}, i32 undef, i32 undef, i32 16)
// SIZE-LABEL: declare double @dx.op.makeDouble.f64

// NO-OLD-LABEL: define void @main()
// NO-OLD-NOT: i32 undef, i32 undef, i32 20)
// NO-OLD-LABEL: declare double @dx.op.makeDouble.f64
