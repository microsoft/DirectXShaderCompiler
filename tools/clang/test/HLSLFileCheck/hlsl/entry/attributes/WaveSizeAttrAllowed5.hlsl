// RUN: %dxc -T lib_6_7 %s 

// This test just checks whether compilation succeeds
// because we shouldn't presume that N() is an entry 
// point, so entry point attributes are ignored.

// CHECK-NOT: error:
// CHECK-NOT: warning:

[WaveSize(64)]
void N(){}
