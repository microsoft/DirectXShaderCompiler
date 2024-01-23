// RUN: %dxc -T cs_6_6 -DNODE -ast-dump %s | FileCheck %s -check-prefixes=AST,AST1,ASTNODE,ASTNODE1
// RUN: %dxc -T cs_6_8 -DNODE -ast-dump %s | FileCheck %s -check-prefixes=AST,AST1,ASTNODE,ASTNODE1
// RUN: %dxc -T lib_6_6 -ast-dump %s | FileCheck %s -check-prefixes=AST,AST1
// RUN: %dxc -T lib_6_8 -DNODE -ast-dump %s | FileCheck %s -check-prefixes=AST,AST1,ASTNODE,ASTNODE1

// RUN: %dxc -T cs_6_8 -DNODE -DRANGE=,64 -ast-dump %s | FileCheck %s -check-prefixes=AST,AST2,ASTNODE,ASTNODE2 -DPREF=0
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64 -ast-dump %s | FileCheck %s -check-prefixes=AST,AST2,ASTNODE,ASTNODE2 -DPREF=0

// RUN: %dxc -T cs_6_8 -DNODE -DRANGE=,64,32 -ast-dump %s | FileCheck %s -check-prefixes=AST,AST2,ASTNODE,ASTNODE2 -DPREF=32
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64,32 -ast-dump %s | FileCheck %s -check-prefixes=AST,AST2,ASTNODE,ASTNODE2 -DPREF=32

// RUN: %dxc -T cs_6_6 -DNODE %s | FileCheck %s -check-prefixes=META,META1
// RUN: %dxc -T cs_6_8 -DNODE %s | FileCheck %s -check-prefixes=META,META1
// RUN: %dxc -T lib_6_6 %s | FileCheck %s -check-prefixes=META,META1
// RUN: %dxc -T lib_6_8 -DNODE %s | FileCheck %s -check-prefixes=META,META1,METANODE,METANODE1

// RUN: %dxc -T cs_6_8 -DNODE -DRANGE=,64 %s | FileCheck %s -check-prefixes=META,META2 -DPREF=0
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64 %s | FileCheck %s -check-prefixes=META,META2,METANODE,METANODE2 -DPREF=0

// RUN: %dxc -T cs_6_8 -DNODE -DRANGE=,64,32 %s | FileCheck %s -check-prefixes=META,META2 -DPREF=32
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64,32 %s | FileCheck %s -check-prefixes=META,META2,METANODE,METANODE2 -DPREF=32

// RUN: %dxc -T lib_6_8 -DNODE %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT1
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT2
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64,32 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT2

// Notes on RUN variations:
//  - Tests cs and lib with SM 6.6 and SM 6.8, with limitations for SM 6.6:
//    - Node shader excluded from lib_6_6, or node entry is not compat.
//      - another way would be to specify exports, but that complicates things,
//        since it will cause second callable main export
//    - Means RDAT checks may assume SM 6.8
//    - Limited to legacy form (1)
//  - -DRANGE used with range form to specify Max and optionally Preferred
//  - -DPREF=N used for FileCheck to check matching preference value, 0 or 32

// AST-LABEL: main 'void ()'
// AST1: -HLSLWaveSizeAttr
// AST1-SAME: 16 0 0
// AST2: -HLSLWaveSizeAttr
// AST2-SAME: 16 64 [[PREF]]

// ASTNODE-LABEL: node 'void ()'
// ASTNODE1: -HLSLWaveSizeAttr
// ASTNODE1-SAME: 16 0 0
// ASTNODE2: -HLSLWaveSizeAttr
// ASTNODE2-SAME: 16 64 [[PREF]]

// META: @main, !"main", null, null, [[PROPS:![0-9]+]]}
// META1: [[PROPS]] = !{
// META1-SAME: i32 11, [[WS:![0-9]+]]
// META2: [[PROPS]] = !{
// META2-SAME: i32 23, [[WS:![0-9]+]]
// META1: [[WS]] = !{i32 16}
// META2: [[WS]] = !{i32 16, i32 64, i32 [[PREF]]}

// METANODE: @node, !"node", null, null, [[PROPS:![0-9]+]]}
// METANODE1: [[PROPS]] = !{
// METANODE1-SAME: i32 11, [[WS]]
// METANODE2: [[PROPS]] = !{
// METANODE2-SAME: i32 23, [[WS]]

// RDAT has no min/max wave count until SM 6.8
// RDAT-LABEL: <0:RuntimeDataFunctionInfo{{.}}> = {
// RDAT: Name: "main"
// RDAT: MinimumExpectedWaveLaneCount: 16
// RDAT1: MaximumExpectedWaveLaneCount: 16
// RDAT2: MaximumExpectedWaveLaneCount: 64

// RDAT-LABEL: <1:RuntimeDataFunctionInfo{{.}}> = {
// RDAT: Name: "node"
// RDAT: MinimumExpectedWaveLaneCount: 16
// RDAT1: MaximumExpectedWaveLaneCount: 16
// RDAT2: MaximumExpectedWaveLaneCount: 64

#ifndef RANGE
#define RANGE
#endif

[shader("compute")]
[wavesize(16 RANGE)]
[numthreads(1,1,8)]
void main() {
}

#ifdef NODE
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,8)]
[NodeDispatchGrid(1,1,1)]
[WaveSize(16 RANGE)]
void node() { }
#endif