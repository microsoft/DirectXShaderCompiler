// RUN: not %dxc -Zi -E main -T cs_6_0 %s 2>&1 | FileCheck %s -check-prefix=CHECK -check-prefix=CHK_DB
// RUN: not %dxc -E main -T cs_6_0 %s 2>&1 | FileCheck %s -check-prefix=CHECK -check-prefix=CHK_NODB

// REQUIRES: dxilver_1_6

// CHK_DB: 20:17: error: Assignment of undefined values to UAV.
// CHK_NODB: 20:17: error: Assignment of undefined values to UAV.

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
