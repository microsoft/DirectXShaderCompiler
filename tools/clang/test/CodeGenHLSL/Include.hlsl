// RUN: %dxc -E main -T ps_6_0 -Vi -I inc %s | StdErrCheck %s



// CHECK: Opening file [
// CHECK: /inc/header.hlsli], stack top [0]

#include "inc/header.hlsli"

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 main( PS_INPUT Input) : SV_TARGET
{
	float4 vDiffuse = g_txDiffuse.Sample( g_samLinear, Input.vTexcoord );
	
	float fLighting = saturate( dot( g_vLightDir, Input.vNormal ) );
	fLighting = max( fLighting, g_fAmbient );
	
	return vDiffuse * fLighting;
}

