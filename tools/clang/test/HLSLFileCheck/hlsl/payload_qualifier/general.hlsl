// RUN: %dxc -T lib_6_6 -D TEST_NUM=0 %s -allow-payload-qualifiers | FileCheck -check-prefix=CHK0 %s
// RUN: %dxc -T lib_6_6 -D TEST_NUM=1 %s -allow-payload-qualifiers | FileCheck -check-prefix=CHK1 %s
// RUN: %dxc -T lib_6_6 -D TEST_NUM=2 %s -allow-payload-qualifiers | FileCheck -check-prefix=CHK2 %s
// RUN: %dxc -T lib_6_3 -D TEST_NUM=3 %s -allow-payload-qualifiers | FileCheck -check-prefix=CHK3 %s
// RUN: %dxc -T lib_6_5 -D TEST_NUM=3 %s | FileCheck -input=stderr -check-prefix=CHK4 %s
// RUN: %dxc -T lib_6_5 -D TEST_NUM=3 %s -allow-payload-qualifiers | FileCheck -check-prefix=CHK5 %s

// CHK0: error: shader must include inout payload structure parameter.
// CHK1: error: ray payload parameter must be declared inout
// CHK2: error: ray payload parameter must be a user defined type with only numeric contents.

// check if we get DXIL and the payload type is there 
// CHK3: error: expected expression
// CHK4: warning: payload access qualifieres are only support for target lib_6_7 and beyond. You can opt-in with the -allow-payload-qualifiers flag. Qualifiers will be dropped.
// CHK5: %struct.Payload = type { i32, i32 }

struct Payload {
    int a;
    int b : out (trace, closesthit);
};

// test if compilation fails if payload is not present
#if TEST_NUM == 0
[shader("miss")]
void Miss(){}
#endif

// test if compilation fails if payload is not inout
#if TEST_NUM == 1
[shader("miss")]
void Miss2( in Payload payload){}
#endif

// test if compilation fails if payload is not a user defined type
#if TEST_NUM == 2
[shader("miss")]
void Miss3(inout int payload){}
#endif

// test if compilation fails because not all payload filds are qualified for lib_6_6
// test if compilation succeeds for lib_6_5 where payload access qualifiers are not required
#if TEST_NUM == 3
[shader("miss")]
void Miss4(inout Payload payload){}
#endif
