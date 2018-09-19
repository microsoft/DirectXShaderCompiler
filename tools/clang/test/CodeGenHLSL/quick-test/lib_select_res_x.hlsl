// RUN: %dxc -T lib_6_x -Od -Zi -auto-binding-space 11 %s | FileCheck %s

// lib_6_x allows phi on resource, targeting offline linking only.
// CHECK: phi %struct.ByteAddressBuffer

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