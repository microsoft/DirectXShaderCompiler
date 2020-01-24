// RUN: %dxc -O0 -T ps_6_0 %s | FileCheck %s
// CHECK: fadd
// CHECK: fadd
// CHECK: fadd
// CHECK: fadd

float4 main(float4 color : A) : SV_Target
{
  float red = color.r;
  float4 outColor = color;
  // Will need to change if there's ever a major version bump
#if defined(__DXC_VERSION_MAJOR) && __DXC_VERSION_MAJOR == 1
  red += 1;
#else
  red -= 1;
#endif

  // Minor version is expected to change fairly frequently. So be conservative
#if defined(__DXC_VERSION_MINOR) && __DXC_VERSION_MINOR >= 0 && __DXC_VERSION_MINOR < 50
  red += 1;
#else
  red -= 1;
#endif

  // Release is either based on the year/month or set to zero for dev builds
#if defined(__DXC_VERSION_RELEASE) && (__DXC_VERSION_RELEASE == 0 ||  __DXC_VERSION_RELEASE > 1900)
  red += 1;
#else
  red -= 1;
#endif

  // The last number varies a lot. In a dev build, Release is 0, and this number
  // is the total number of commits since the beginning of the project.
  // which can be expected to be more than 1000 at least.
#if defined(__DXC_VERSION_RELEASE) && (__DXC_VERSION_RELEASE > 0 ||  __DXC_VERSION_COMMITS > 1000)
  red += 1;
#else
  red -= 1;
#endif
  outColor.r = red;
  return outColor;
}
