// RUN: %dxc -E main -T ps_6_0 -rootsig-define RS %s
// TODO: No check lines found, we should update this
// Test root signature define empty

#define RS 

float main(float i : I) : SV_Target
{
  return i;
}

