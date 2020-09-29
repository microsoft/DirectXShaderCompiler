// Run: %dxc -T ps_6_0 -E main -Zi

// It is impossible to check SPIR-V output because of the source code
// dump, which will always contain the statements to check.
// This test just checks whether it fails because of errors or not.

#line 1 "non_existing_file.txt"

struct PSInput
{
  float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
  return input.color;
}
