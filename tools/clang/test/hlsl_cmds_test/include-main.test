// Test display include process with /Vi
// RUN: %dxc -T ps_6_0 -Vi -I inc %s | FileCheck %s --check-prefix=VI

// Test file with relative path and include
// RUN: %dxc -T ps_6_0 -Vi -I .\inc .\subfolder\include-main.hlsl | FileCheck %s --check-prefix=VI

// VI:; Opening file [
// VI-SAME:inc\include-declarations.h], stack top [0]

#include "include-declarations.h"

void main() {
}
