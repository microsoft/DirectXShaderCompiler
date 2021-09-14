// RUN: %dxc -T cs_6_0 %s | %FileCheck %s

// CHECK: define void @main()
// CHECK-NOT: i64
// CHECK: ret void

RWByteAddressBuffer bab;

[numthreads(4,1,1)]
void main() {
    uint acc = 0;
    acc += (uint)WaveActiveMax(10);
    acc += (uint)WavePrefixSum(5);
    acc += (uint)WavePrefixProduct(-5);
    acc += (uint)WavePrefixSum((bab.Load(0) == 0) ? 1 : 0);
    acc += (uint)QuadReadAcrossX(1);
    acc += (uint)QuadReadAcrossY(-1);
    acc += (uint)QuadReadLaneAt(2, 3);
    acc += (uint)QuadReadLaneAt(-2, 1);
    acc += (uint)QuadReadLaneAt((bab.Load(1) == 0) ? 5 : -3, bab.Load(2) & 3);
    acc += (uint)QuadReadAcrossDiagonal((bab.Load(3) == 0) ? 1 : 0);
    acc += (uint)abs(-7);
    bab.Store(32, acc);
}
