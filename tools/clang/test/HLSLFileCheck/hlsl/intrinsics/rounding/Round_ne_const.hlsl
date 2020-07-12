// RUN: %dxc -T vs_6_0 -E fr -DVAL=0.5  %s | %FileCheck -check-prefix=FLT_RND_1 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=-0.5  %s | %FileCheck -check-prefix=FLT_RND_2 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=1.5  %s | %FileCheck -check-prefix=FLT_RND_3 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=-1.5  %s | %FileCheck -check-prefix=FLT_RND_4 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=1.6  %s | %FileCheck -check-prefix=FLT_RND_5 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=1.3  %s | %FileCheck -check-prefix=FLT_RND_6 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=0.5 -HV 2016 %s | %FileCheck -check-prefix=FLT_RND_7 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=-0.5 -HV 2016 %s  | %FileCheck -check-prefix=FLT_RND_8 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=1.5 -HV 2016 %s  | %FileCheck -check-prefix=FLT_RND_9 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=-1.5 -HV 2016 %s  | %FileCheck -check-prefix=FLT_RND_10 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=1.6 -HV 2016  %s | %FileCheck -check-prefix=FLT_RND_11 %s
// RUN: %dxc -T vs_6_0 -E fr -DVAL=1.3 -HV 2016  %s | %FileCheck -check-prefix=FLT_RND_12 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=0.5  %s | %FileCheck -check-prefix=DBL_RND_1 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=-0.5  %s | %FileCheck -check-prefix=DBL_RND_2 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=1.5  %s | %FileCheck -check-prefix=DBL_RND_3 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=-1.5  %s | %FileCheck -check-prefix=DBL_RND_4 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=1.6  %s | %FileCheck -check-prefix=DBL_RND_5 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=1.3  %s | %FileCheck -check-prefix=DBL_RND_6 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=0.5 -HV 2016 %s | %FileCheck -check-prefix=DBL_RND_7 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=-0.5 -HV 2016 %s  | %FileCheck -check-prefix=DBL_RND_8 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=1.5 -HV 2016 %s  | %FileCheck -check-prefix=DBL_RND_9 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=-1.5 -HV 2016 %s  | %FileCheck -check-prefix=DBL_RND_10 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=1.6 -HV 2016  %s | %FileCheck -check-prefix=DBL_RND_11 %s
// RUN: %dxc -T vs_6_0 -E dr -DVAL=1.3 -HV 2016  %s | %FileCheck -check-prefix=DBL_RND_12 %s

// Both FXC and DXC would apply nearest even rounding mode on variables, but on compile-time constants
// it would apply away from zero for midway values. This would cause differences in results in cases such
// as round(x) (where x is 0.5) and round(0.5) where the former would evaluate to 1 and latter would
// evaluate to 0.
// 
// For compatibility with FXC,  DXC still preserve above behavior for language version 2016 or below.
// However for newer language version, DXC would always use nearest even for round() intrinsic in all
// cases.


// FLT_RND_1: call void @dx.op.storeOutput{{.*}} float 0.000000e+00
// FLT_RND_2: call void @dx.op.storeOutput{{.*}} float -0.000000e+00
// FLT_RND_3: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// FLT_RND_4: call void @dx.op.storeOutput{{.*}} float -2.000000e+00
// FLT_RND_5: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// FLT_RND_6: call void @dx.op.storeOutput{{.*}} float 1.000000e+00

// FLT_RND_7: select i1 %{{.*}}, float 0.000000e+00, float 1.000000e+00
// FLT_RND_8: select i1 %{{.*}}, float -0.000000e+00, float -1.000000e+00
// FLT_RND_9: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// FLT_RND_10: call void @dx.op.storeOutput{{.*}} float -2.000000e+00
// FLT_RND_11: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// FLT_RND_12: call void @dx.op.storeOutput{{.*}} float 1.000000e+00

// DBL_RND_1: call void @dx.op.storeOutput{{.*}} float 0.000000e+00
// DBL_RND_2: call void @dx.op.storeOutput{{.*}} float -0.000000e+00
// DBL_RND_3: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// DBL_RND_4: call void @dx.op.storeOutput{{.*}} float -2.000000e+00
// DBL_RND_5: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// DBL_RND_6: call void @dx.op.storeOutput{{.*}} float 1.000000e+00

// DBL_RND_7: select i1 %{{.*}}, float 0.000000e+00, float 1.000000e+00
// DBL_RND_8: select i1 %{{.*}}, float -0.000000e+00, float -1.000000e+00
// DBL_RND_9: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// DBL_RND_10: call void @dx.op.storeOutput{{.*}} float -2.000000e+00
// DBL_RND_11: call void @dx.op.storeOutput{{.*}} float 2.000000e+00
// DBL_RND_12: call void @dx.op.storeOutput{{.*}} float 1.000000e+00

float fr(float f : INPUT) : OUTPUT {
  if (f == VAL)
    return round(f);
  else
    return round(VAL);
}

RWStructuredBuffer<double> buf;

float dr() : OUTPUT {
  double d = buf[0];
  if (d == VAL)
    return round(d);
  else
    return round(VAL);
}