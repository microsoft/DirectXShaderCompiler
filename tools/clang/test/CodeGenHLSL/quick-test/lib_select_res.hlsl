// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure createHandleFromResourceStructForLib is used for resource.
// CHECK:call %dx.types.Handle @dx.op.createHandleFromResourceStructForLib.struct.ByteAddressBuffer(i32 160
// CHECK:call %dx.types.Handle @dx.op.createHandleFromResourceStructForLib.struct.ByteAddressBuffer(i32 160
// CHECK:call %dx.types.Handle @dx.op.createHandleFromResourceStructForLib.struct.RWByteAddressBuffer(i32 160

RWByteAddressBuffer outputBuffer : register(u0);
ByteAddressBuffer ReadBuffer : register(t0);
ByteAddressBuffer ReadBuffer1 : register(t1);

void test( uint cond)
{
	ByteAddressBuffer buffer = ReadBuffer;
        if (cond > 2)
           buffer = ReadBuffer1;

	uint v= buffer.Load(0);
    outputBuffer.Store(0, v);
}