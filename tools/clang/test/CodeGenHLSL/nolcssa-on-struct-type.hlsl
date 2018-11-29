// RUN: %dxc -E main -T ps_6_0 /Zpr /O3 %s | FileCheck %s
// CHECK-NOT: phi %dx.types.CBufRet.i32

cbuffer cb
{
 int cb_a;
 int cb_b;
 bool cb_c[1];
}

static const struct
{
 int a;
 int b;
 bool c[1];
} cbstruct = { cb_a, cb_b, cb_c };


bool IsPos( float3 pos )
{
 float maxpos = max(pos.x, max(pos.y, pos.z) );
 float minpos = min(pos.x, min(pos.y, pos.z) );
 return ( maxpos < 32 && minpos > 0 );
}

float3 GetPos()
{
 [branch]
 if(cbstruct.c[cbstruct.b])
 {
  return float3(0,0,0);
 }
 else
 {
  return float3(1,1,1);
 }
}

int GetIdx( )
{
 for( int i=0; i< 4 ; i++ )
 {
  if( i == cbstruct.b )
  {
   return -1;
  }
  float3 pos = GetPos();
  if( IsPos(pos) )
  {
   return i;
  }
 }

 return -1;
}

float GetColor()
{
 int idx = GetIdx(); 
 if( idx == cbstruct.a )
 {
   return 1.0;
 }
 return 0.0;
}


void main(out float4 OutColor : SV_Target0)
{
 float x = GetColor();
 [branch]
 if ( x < 0.0001f )
 {
  OutColor = 0.0f;
 }
}