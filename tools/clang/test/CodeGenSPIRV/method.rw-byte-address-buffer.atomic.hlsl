// Run: %dxc -T ps_6_0 -E main

// Note: According to HLSL reference (https://msdn.microsoft.com/en-us/library/windows/desktop/ff471475(v=vs.85).aspx),
// all RWByteAddressBuffer atomic methods must take unsigned integers as parameters.

RWByteAddressBuffer myBuffer;

float4 main() : SV_Target
{
    uint originalVal;

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicIAdd %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedAdd(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicIAdd %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedAdd(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicAnd %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedAnd(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicAnd %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedAnd(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicOr %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedOr(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicOr %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedOr(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicXor %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedXor(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicXor %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedXor(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicUMax %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedMax(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicUMax %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedMax(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicUMax %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedMax(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicUMax %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedMax(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicUMin %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedMin(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicUMin %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedMin(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:        {{%\d+}} = OpAtomicUMin %uint [[ptr]] %uint_1 %uint_0 %uint_42
    myBuffer.InterlockedMin(16, 42);
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicUMin %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedMin(16, 42, originalVal);

    // .InterlockedExchnage() has no two-parameter overload.
// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicExchange %uint [[ptr]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedExchange(16, 42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicCompareExchange %uint [[ptr]] %uint_1 %uint_0 %uint_0 %uint_42 %uint_30
// CHECK-NEXT:                   OpStore %originalVal [[val]]
    myBuffer.InterlockedCompareExchange(/*offset=*/16, /*compare_value=*/30, /*value=*/42, originalVal);

// CHECK:      [[offset:%\d+]] = OpShiftRightLogical %uint %uint_16 %uint_2
// CHECK-NEXT:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %myBuffer %uint_0 [[offset]]
// CHECK-NEXT:    [[val:%\d+]] = OpAtomicCompareExchange %uint [[ptr]] %uint_1 %uint_0 %uint_0 %uint_42 %uint_30
// CHECK-NOT:                    [[val]]
    myBuffer.InterlockedCompareStore(/*offset=*/16, /*compare_value=*/30, /*value=*/42);

    return 1.0;
}
