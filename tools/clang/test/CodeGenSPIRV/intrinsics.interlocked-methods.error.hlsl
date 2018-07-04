// Run: %dxc -T cs_6_0 -E main

groupshared float MyFloat;
RWBuffer<float> MyBuffer;

typedef int MyIntegerType;
RWStructuredBuffer<MyIntegerType> MySBuffer;

[numthreads(1, 1, 1)]
void main()
{
  InterlockedAdd(MyFloat, 1);
  InterlockedCompareStore(MyBuffer[0], MySBuffer[0], MySBuffer[1]);
  InterlockedXor(MySBuffer[2], 5);
}

// CHECK:     :12:18: error: can only perform atomic operations on scalar integer values
// CHECK:     :13:27: error: can only perform atomic operations on scalar integer values
// CHECK-NOT:         error: can only perform atomic operations on scalar integer values
