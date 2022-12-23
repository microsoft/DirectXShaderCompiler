
// RUN: %dxc %S/Inputs/smoke.hlsl /D "semantic = SV_Position" /T vs_6_0 /Zi /Qembed_debug /DDX12 /Fo %t.dxa.cso

// RUN: %dxa %t.dxa.cso -listfiles | FileCheck %s --check-prefix=FILES
// FILES:smoke.hlsl

// RUN: %dxa %t.dxa.cso -listparts | FileCheck %s --check-prefix=PARTS
// PARTS-DAG:DXIL
// PARTS-DAG:ILDB
// PARTS-DAG:RTS0
// PARTS-DAG:PSV0
// PARTS-DAG:STAT
// PARTS-DAG:ILDN
// PARTS-DAG:HASH
// PARTS-DAG:ISG1
// PARTS-DAG:OSG1


// RUN: %dxa %t.dxa.cso -extractpart dbgmodule -o %t.dxa.cso.dbgmodule

// RUN: %dxa %t.dxa.cso.dbgmodule -listfiles | FileCheck %s --check-prefix=DBG_FILES
// DBG_FILES:smoke.hlsl

// RUN: %dxa %t.dxa.cso.dbgmodule -extractfile=* | FileCheck %s --check-prefix=EXTRACT_FILE
// EXTRACT_FILE:float4 main()


// RUN: %dxa %t.dxa.cso -extractpart module -o %t.dxa.cso.plain.bc
// RUN: %dxa %t.dxa.cso.plain.bc -o %t.rebuilt-container.cso
// RUN: %dxc -dumpbin %t.rebuilt-container.cso | FileCheck %s --check-prefix=REBUILD

// RUN: %dxc -dumpbin %t.dxa.cso -Fc %t.dxa.ll
// RUN: %dxa %t.dxa.ll -o %t.rebuilt-container2.cso
// RUN: %dxc -dumpbin %t.rebuilt-container2.cso | FileCheck %s --check-prefix=REBUILD

// REBUILD:define void @main()
