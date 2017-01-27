// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: object's templated type must have at least one element

struct Foo {
  float2 a;
  float b;
  float c;
  float d[2];
};


struct Empty {};
Buffer<Empty> eb;

RWBuffer< int > g_Intensities : register(u1);

groupshared int sharedData;

[ numthreads( 64, 2, 2 ) ]
void main( uint GI : SV_GroupIndex)
{
        eb[GI];
	sharedData = GI;
	int rtn;
	InterlockedAdd(sharedData, g_Intensities[GI], rtn);
	g_Intensities[GI] = rtn + sharedData;
}