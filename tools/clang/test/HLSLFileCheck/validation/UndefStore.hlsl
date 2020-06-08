// RUN: %dxc -Zi -E main -T cs_6_0 %s | FileCheck %s -check-prefix=CHECK -check-prefix=CHK_DB
// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s -check-prefix=CHECK -check-prefix=CHK_NODB

// CHK_DB: 18:17: error: Assignment of undefined values to UAV.
// CHK_NODB: Function: main: error: Assignment of undefined values to UAV. Use /Zi for source location.

RWBuffer<uint> output;

uint Add(uint a, uint b)
{
	return a + b;
}

[numthreads(64,1,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint sum = Add(sum, (uint)DTid.x);	// Deliberate use of uninitialised variable 'sum'
	output[DTid.x] = sum;
}