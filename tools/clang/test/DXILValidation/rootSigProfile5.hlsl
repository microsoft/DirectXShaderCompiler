// RUN: %dxc -E RS -T rootsig_1_0 %s
// TODO: No check lines found, we should update this
// Test root signature compilation from expanded macro.

#define YYY "DescriptorTable" "(SRV(t3))"
#define RS YYY
