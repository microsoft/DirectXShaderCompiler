// Skip hash stability becase rootsig not generate hash part
// UNSUPPORTED: hash_stability

// RUN:%dxc -T rootsig_1_1 %s -rootsig-define main -Fo %t
// RUN:%dxa  -listparts %t | FileCheck %s

#define main "CBV(b0)"

// CHECK:Part count: 1
// CHECK:#0 - RTS0
