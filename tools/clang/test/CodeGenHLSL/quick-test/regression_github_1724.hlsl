// RUN: %dxc /T vs_6_0 /E main %s
// Was crashing in SROA_Parameter_HLSL due to the memcpy instruction
// between the VsInput structs erased while an IRBuilder was
// pointing one of its source bitcasts as the insertion point.

struct VsInput
{
	uint instanceId : SV_InstanceID;
	uint vertexId : SV_VertexID;
};

static VsInput __vsInput;

void main(VsInput inputs)
{
	__vsInput = inputs; // This line triggers compiler crash
}