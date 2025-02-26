// RUN: %dxc -T lib_6_9 %s -verify

struct [raypayload] Payload {
  HitObject hit : write(caller) : read(closesthit);
};

// expected-error@+3{{payload parameter 'pldCalleeArg' must be a user-defined type composed of only numeric types}}
[shader("closesthit")]
void
main(inout Payload pldCalleeArg, in BuiltInTriangleIntersectionAttributes attrs) {
}
