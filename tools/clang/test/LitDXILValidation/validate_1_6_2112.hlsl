// REQUIRES: dxilval-v1.6.2112

// Verify that the older DLL is being loaded, and that
// due to it being older, it errors on something that would
// be accepted in newer validators.
// Specifically, the v1.6.2112 validator would emit the 
// below validation error, while validators newer than 1.6
// would be able to validate the module.

// RUN: not dxc -T cs_6_0 -validator-version 1.7 %s 2>&1 | FileCheck %s
// CHECK: error: Validator version in metadata (1.7) is not supported; maximum: (1.6).
[numthreads(1, 1, 1)]
void main() {}
