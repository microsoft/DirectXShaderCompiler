// RUN: %dxc -Od -E main -T ps_6_0 %s /E main_ps | FileCheck %s

// Regression test for bug #2269
// The max iteration attempt for loop unroll was initialized once but
// modified by each loop pass run if an explicit loop bound/trip count
// is available. So if a loop was unrolled first, the max trip count 
// would be cached, so if a loop that need iterative unrolling is needed
// later, it would be bound with the (potentially too low) max attempt.

// CHECK: @main_ps


#define MM 2
#define KK 3
#define NN 1    // change from 1 to 2 and no errors

float c1()
{
    float a = 1.f;
    
    [unroll]
    for( uint i = 0 ; i < MM && a > 1e-4; ++i )
    //for( uint i = 0 ; i < MM;  ++i )          // uncomment this -> no errors
    {
        float b = 0.01;     // comment this out -> no errors
        a *= 1.0f - b;
    }
    return 0;
}

float c3()
{
    [unroll] for (int i = 0; i < NN; ++i)         // comment this out -> no errors
    {
        [unroll] for (int j = 0; j < (KK - 1); ++j)     // comment this out -> no errors
        {
        }
    }
    
	return 0;
}

float main_ps() : SV_Target
{
    return c1() * c3();
}
