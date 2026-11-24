// REQUIRES: dxil-1-10

// RUN: %dxc -T cs_6_10 -E main -HV 2021 -enable-16bit-types -verify %s
// RUN: %dxc -DTEST_DXIL -T cs_6_10 -E main -HV 2021 -enable-16bit-types -verify %s
// RUN: %dxc %if spirv %{ -spirv -fspv-target-env=vulkan1.3 %} -T cs_6_2 -E main -HV 2021 -enable-16bit-types -verify %s

// expected-no-diagnostics

#include <enable_if>
#include <enable_if>
#include <type_traits>
#include <type_traits>
#include <vector_utils>
#include <vector_utils>

#ifdef __spirv__
#include <vk/spirv.h>
#include <vk/spirv.h>
#include <vk/opcode_selector.h>
#include <vk/opcode_selector.h>
// The public header also covers cooperative_matrix.impl.
#include <vk/khr/cooperative_matrix.h>
#include <vk/khr/cooperative_matrix.h>
#endif

#ifdef TEST_DXIL
#include <dx/linalg.h>
#include <dx/linalg.h>
#endif

[numthreads(1, 1, 1)] void main() {}
