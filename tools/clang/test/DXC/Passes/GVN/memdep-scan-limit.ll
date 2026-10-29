; RUN: %dxopt %s -hlsl-passes-resume -gvn -S | FileCheck %s --check-prefix=GVN
; RUN: %dxopt %s -hlsl-passes-resume -basicaa -dse -S | FileCheck %s --check-prefix=DSE

; MemoryDependenceAnalysis scans at most 100 instructions, including the
; dependency itself. Keep coverage for passes used by DXC that rely on this
; limit. MemCpyOpt also uses MemDep in LLVM, but DXC disables that pass because
; HLSL does not allow memcpy. The live add chains ensure that the filler
; survives GVN and DSE.

target datalayout = "e-m:e-p:32:32-i64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; With 98 intervening instructions, MemDep reaches the defining access.
;
; GVN-LABEL: define i32 @within_limit(
; GVN-COUNT-1: load i32, i32* %load.ptr
;
; DSE-LABEL: define i32 @within_limit(
; DSE-NOT: store i32 1, i32* %store.ptr
; DSE: store i32 2, i32* %store.ptr
define i32 @within_limit(i32* noalias %load.ptr, i32* noalias %store.ptr,
                         i32 %seed) {
entry:
  %load.first = load i32, i32* %load.ptr
  store i32 1, i32* %store.ptr
  %add.pre = add i32 %seed, 1
  %add.0 = add i32 %add.pre, 1
  %add.1 = add i32 %add.0, 1
  %add.2 = add i32 %add.1, 1
  %add.3 = add i32 %add.2, 1
  %add.4 = add i32 %add.3, 1
  %add.5 = add i32 %add.4, 1
  %add.6 = add i32 %add.5, 1
  %add.7 = add i32 %add.6, 1
  %add.8 = add i32 %add.7, 1
  %add.9 = add i32 %add.8, 1
  %add.10 = add i32 %add.9, 1
  %add.11 = add i32 %add.10, 1
  %add.12 = add i32 %add.11, 1
  %add.13 = add i32 %add.12, 1
  %add.14 = add i32 %add.13, 1
  %add.15 = add i32 %add.14, 1
  %add.16 = add i32 %add.15, 1
  %add.17 = add i32 %add.16, 1
  %add.18 = add i32 %add.17, 1
  %add.19 = add i32 %add.18, 1
  %add.20 = add i32 %add.19, 1
  %add.21 = add i32 %add.20, 1
  %add.22 = add i32 %add.21, 1
  %add.23 = add i32 %add.22, 1
  %add.24 = add i32 %add.23, 1
  %add.25 = add i32 %add.24, 1
  %add.26 = add i32 %add.25, 1
  %add.27 = add i32 %add.26, 1
  %add.28 = add i32 %add.27, 1
  %add.29 = add i32 %add.28, 1
  %add.30 = add i32 %add.29, 1
  %add.31 = add i32 %add.30, 1
  %add.32 = add i32 %add.31, 1
  %add.33 = add i32 %add.32, 1
  %add.34 = add i32 %add.33, 1
  %add.35 = add i32 %add.34, 1
  %add.36 = add i32 %add.35, 1
  %add.37 = add i32 %add.36, 1
  %add.38 = add i32 %add.37, 1
  %add.39 = add i32 %add.38, 1
  %add.40 = add i32 %add.39, 1
  %add.41 = add i32 %add.40, 1
  %add.42 = add i32 %add.41, 1
  %add.43 = add i32 %add.42, 1
  %add.44 = add i32 %add.43, 1
  %add.45 = add i32 %add.44, 1
  %add.46 = add i32 %add.45, 1
  %add.47 = add i32 %add.46, 1
  %add.48 = add i32 %add.47, 1
  %add.49 = add i32 %add.48, 1
  %add.50 = add i32 %add.49, 1
  %add.51 = add i32 %add.50, 1
  %add.52 = add i32 %add.51, 1
  %add.53 = add i32 %add.52, 1
  %add.54 = add i32 %add.53, 1
  %add.55 = add i32 %add.54, 1
  %add.56 = add i32 %add.55, 1
  %add.57 = add i32 %add.56, 1
  %add.58 = add i32 %add.57, 1
  %add.59 = add i32 %add.58, 1
  %add.60 = add i32 %add.59, 1
  %add.61 = add i32 %add.60, 1
  %add.62 = add i32 %add.61, 1
  %add.63 = add i32 %add.62, 1
  %add.64 = add i32 %add.63, 1
  %add.65 = add i32 %add.64, 1
  %add.66 = add i32 %add.65, 1
  %add.67 = add i32 %add.66, 1
  %add.68 = add i32 %add.67, 1
  %add.69 = add i32 %add.68, 1
  %add.70 = add i32 %add.69, 1
  %add.71 = add i32 %add.70, 1
  %add.72 = add i32 %add.71, 1
  %add.73 = add i32 %add.72, 1
  %add.74 = add i32 %add.73, 1
  %add.75 = add i32 %add.74, 1
  %add.76 = add i32 %add.75, 1
  %add.77 = add i32 %add.76, 1
  %add.78 = add i32 %add.77, 1
  %add.79 = add i32 %add.78, 1
  %add.80 = add i32 %add.79, 1
  %add.81 = add i32 %add.80, 1
  %add.82 = add i32 %add.81, 1
  %add.83 = add i32 %add.82, 1
  %add.84 = add i32 %add.83, 1
  %add.85 = add i32 %add.84, 1
  %add.86 = add i32 %add.85, 1
  %add.87 = add i32 %add.86, 1
  %add.88 = add i32 %add.87, 1
  %add.89 = add i32 %add.88, 1
  %add.90 = add i32 %add.89, 1
  %add.91 = add i32 %add.90, 1
  %add.92 = add i32 %add.91, 1
  %add.93 = add i32 %add.92, 1
  %add.94 = add i32 %add.93, 1
  %add.95 = add i32 %add.94, 1
  %load.second = load i32, i32* %load.ptr
  store i32 2, i32* %store.ptr
  %loads = add i32 %load.first, %load.second
  %result = add i32 %loads, %add.95
  ret i32 %result
}

; With 99 intervening instructions, MemDep stops before the defining access.
;
; GVN-LABEL: define i32 @outside_limit(
; GVN: [[LOAD_FIRST:%.*]] = load i32, i32* %load.ptr
; GVN: [[LOAD_SECOND:%.*]] = load i32, i32* %load.ptr
; GVN: add i32 [[LOAD_FIRST]], [[LOAD_SECOND]]
;
; DSE-LABEL: define i32 @outside_limit(
; DSE: store i32 1, i32* %store.ptr
; DSE: store i32 2, i32* %store.ptr
define i32 @outside_limit(i32* noalias %load.ptr, i32* noalias %store.ptr,
                          i32 %seed) {
entry:
  %load.first = load i32, i32* %load.ptr
  store i32 1, i32* %store.ptr
  %add.pre = add i32 %seed, 1
  %add.0 = add i32 %add.pre, 1
  %add.1 = add i32 %add.0, 1
  %add.2 = add i32 %add.1, 1
  %add.3 = add i32 %add.2, 1
  %add.4 = add i32 %add.3, 1
  %add.5 = add i32 %add.4, 1
  %add.6 = add i32 %add.5, 1
  %add.7 = add i32 %add.6, 1
  %add.8 = add i32 %add.7, 1
  %add.9 = add i32 %add.8, 1
  %add.10 = add i32 %add.9, 1
  %add.11 = add i32 %add.10, 1
  %add.12 = add i32 %add.11, 1
  %add.13 = add i32 %add.12, 1
  %add.14 = add i32 %add.13, 1
  %add.15 = add i32 %add.14, 1
  %add.16 = add i32 %add.15, 1
  %add.17 = add i32 %add.16, 1
  %add.18 = add i32 %add.17, 1
  %add.19 = add i32 %add.18, 1
  %add.20 = add i32 %add.19, 1
  %add.21 = add i32 %add.20, 1
  %add.22 = add i32 %add.21, 1
  %add.23 = add i32 %add.22, 1
  %add.24 = add i32 %add.23, 1
  %add.25 = add i32 %add.24, 1
  %add.26 = add i32 %add.25, 1
  %add.27 = add i32 %add.26, 1
  %add.28 = add i32 %add.27, 1
  %add.29 = add i32 %add.28, 1
  %add.30 = add i32 %add.29, 1
  %add.31 = add i32 %add.30, 1
  %add.32 = add i32 %add.31, 1
  %add.33 = add i32 %add.32, 1
  %add.34 = add i32 %add.33, 1
  %add.35 = add i32 %add.34, 1
  %add.36 = add i32 %add.35, 1
  %add.37 = add i32 %add.36, 1
  %add.38 = add i32 %add.37, 1
  %add.39 = add i32 %add.38, 1
  %add.40 = add i32 %add.39, 1
  %add.41 = add i32 %add.40, 1
  %add.42 = add i32 %add.41, 1
  %add.43 = add i32 %add.42, 1
  %add.44 = add i32 %add.43, 1
  %add.45 = add i32 %add.44, 1
  %add.46 = add i32 %add.45, 1
  %add.47 = add i32 %add.46, 1
  %add.48 = add i32 %add.47, 1
  %add.49 = add i32 %add.48, 1
  %add.50 = add i32 %add.49, 1
  %add.51 = add i32 %add.50, 1
  %add.52 = add i32 %add.51, 1
  %add.53 = add i32 %add.52, 1
  %add.54 = add i32 %add.53, 1
  %add.55 = add i32 %add.54, 1
  %add.56 = add i32 %add.55, 1
  %add.57 = add i32 %add.56, 1
  %add.58 = add i32 %add.57, 1
  %add.59 = add i32 %add.58, 1
  %add.60 = add i32 %add.59, 1
  %add.61 = add i32 %add.60, 1
  %add.62 = add i32 %add.61, 1
  %add.63 = add i32 %add.62, 1
  %add.64 = add i32 %add.63, 1
  %add.65 = add i32 %add.64, 1
  %add.66 = add i32 %add.65, 1
  %add.67 = add i32 %add.66, 1
  %add.68 = add i32 %add.67, 1
  %add.69 = add i32 %add.68, 1
  %add.70 = add i32 %add.69, 1
  %add.71 = add i32 %add.70, 1
  %add.72 = add i32 %add.71, 1
  %add.73 = add i32 %add.72, 1
  %add.74 = add i32 %add.73, 1
  %add.75 = add i32 %add.74, 1
  %add.76 = add i32 %add.75, 1
  %add.77 = add i32 %add.76, 1
  %add.78 = add i32 %add.77, 1
  %add.79 = add i32 %add.78, 1
  %add.80 = add i32 %add.79, 1
  %add.81 = add i32 %add.80, 1
  %add.82 = add i32 %add.81, 1
  %add.83 = add i32 %add.82, 1
  %add.84 = add i32 %add.83, 1
  %add.85 = add i32 %add.84, 1
  %add.86 = add i32 %add.85, 1
  %add.87 = add i32 %add.86, 1
  %add.88 = add i32 %add.87, 1
  %add.89 = add i32 %add.88, 1
  %add.90 = add i32 %add.89, 1
  %add.91 = add i32 %add.90, 1
  %add.92 = add i32 %add.91, 1
  %add.93 = add i32 %add.92, 1
  %add.94 = add i32 %add.93, 1
  %add.95 = add i32 %add.94, 1
  %add.96 = add i32 %add.95, 1
  %load.second = load i32, i32* %load.ptr
  store i32 2, i32* %store.ptr
  %loads = add i32 %load.first, %load.second
  %result = add i32 %loads, %add.96
  ret i32 %result
}
