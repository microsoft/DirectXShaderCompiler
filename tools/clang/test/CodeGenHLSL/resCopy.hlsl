// RUN: %dxc -E main  -T cs_6_0 %s

RWBuffer<uint> uav1;
RWBuffer<uint> uav2;

[ numthreads( 64, 2, 2 ) ]
void main( uint GI : SV_GroupIndex)
{
    RWBuffer<uint> u = uav1;
    u[GI] = GI;
    u = uav2;
    u[GI] = GI+1;
}
