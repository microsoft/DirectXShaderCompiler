// RUN: %dxc /Tps_6_2 /Eps_main /Zpc /O3 /Zi > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry

struct MyStruct1 
{     
    float var [ 1 ] ; 
} ; 

struct MyStruct2 
{ 
    MyStruct1 internal [ 1 ] ; 
} ; 

void ps_main ( ) 
{ 
    MyStruct1 mystruct1 [ 1 ] ;
    MyStruct2 mystruct2 ;
    mystruct2.internal = mystruct1 ;
} 