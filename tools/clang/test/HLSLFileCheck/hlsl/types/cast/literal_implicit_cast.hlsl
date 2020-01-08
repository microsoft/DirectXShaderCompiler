// RUN: %dxc -E t1 -DT=float -T vs_6_2 %s | FileCheck -check-prefix=T1_FLT %s
// RUN: %dxc -E t1 -DT=min16float -T vs_6_2 %s | FileCheck -check-prefix=T1_MINFLT %s
// RUN: %dxc -E t1 -DT=half -enable-16bit-types -T vs_6_2 %s | FileCheck -check-prefix=T1_HALF %s
// RUN: %dxc -E t1 -DT=int -T vs_6_2 %s | FileCheck -check-prefix=T1_INT %s
// RUN: %dxc -E t1 -DT=bool -T vs_6_2 %s | FileCheck -check-prefix=T1_BOOL %s
// RUN: %dxc -E t2 -DT=float -T vs_6_2 %s | FileCheck -check-prefix=T2_FLT %s
// RUN: %dxc -E t2 -DT=min16float -T vs_6_2 %s | FileCheck -check-prefix=T2_MINFLT %s
// RUN: %dxc -E t2 -DT=half -enable-16bit-types -T vs_6_2 %s | FileCheck -check-prefix=T2_HALF %s
// RUN: %dxc -E t2 -DT=int -T vs_6_2 %s | FileCheck -check-prefix=T2_INT %s

// T1_FLT: float 1.000000e+00, float 0.000000e+00
// T1_FLT: float 2.000000e+00, float 3.000000e+00
// T1_FLT: float 4.000000e+00, float 6.000000e+00
// T1_FLT: float 5.000000e+00, float 7.000000e+00

// T1_MINFLT: half 0xH3C00, half 0xH0000
// T1_MINFLT: half 0xH4000, half 0xH4200
// T1_MINFLT: half 0xH4400, half 0xH4600
// T1_MINFLT: half 0xH4500, half 0xH4700

// T1_HALF: half 0xH3C00, half 0xH0000
// T1_HALF: half 0xH4000, half 0xH4200
// T1_HALF: half 0xH4400, half 0xH4600
// T1_HALF: half 0xH4500, half 0xH4700

// T1_INT: double 1.000000e+00, double 0.000000e+00
// T1_INT: double 2.000000e+00, double 3.000000e+00
// T1_INT: double 4.000000e+00, double 6.000000e+00
// T1_INT: double 5.000000e+00, double 7.000000e+00

// T1_BOOL: double 1.000000e+00, double 0.000000e+00
// T1_BOOL: double 2.000000e+00, double 3.000000e+00
// T1_BOOL: double 4.000000e+00, double 6.000000e+00
// T1_BOOL: double 5.000000e+00, double 7.000000e+00

T t1 (bool4 i : IN) : OUT
{
	return (i.x ? 1.0 : 0.0)
	* (i.y ? 2.0 : 3.0)
	/ (i.z ? 4.0 : 6.0)
	- (i.w ? 5.0 : 7.0);
}

// T2_FLT: float 8.000000e+00, float 7.000000e+00
// T2_MINFLT: half 0xH4800, half 0xH4700
// T2_HALF: half 0xH4800, half 0xH4700
// T2_INT: i32 8, i32 7

T t2 (bool4 i : IN) : OUT
{
	return (i.x ? 1.0 : 0.0) + 7.0;
}


