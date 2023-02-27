// RUN: %dxc %s /T ps_6_0 -opt-disable sink -Odump | FileCheck %s -check-prefix=NO_SINK
// RUN: %dxc %s /T ps_6_0                   -Odump | FileCheck %s -check-prefix=SINK

// NO_SINK: -instcombine,NoSink=1
// SINK:    -instcombine,NoSink=1

float main() : SV_Target {
    return 0;
}