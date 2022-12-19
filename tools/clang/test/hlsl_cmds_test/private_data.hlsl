// Set private data
// RUN: %echo private data > %t.private.txt
// RUN: %dxc %S/smoke.hlsl /D "semantic = SV_Position" /T vs_6_0 /Zi /Qembed_debug /DDX12 /Fo %t.private.smoke.cso

// RUN: %dxc %t.private.smoke.cso /dumpbin /setprivate %t.private.txt /Fo %t.private.cso

// RUN: %dxc %t.private.cso /dumpbin /Qstrip_priv /Fo %t.noprivate.cso

// RUN: %dxc %t.private.cso /dumpbin /getprivate %t.private1.txt
// RUN: FileCheck %s --input-file=%t.private1.txt --check-prefix=PRIVATE1
// PRIVATE1:private data

// Appending and removing private data blob roundtrip
// RUN: FC %t.private.smoke.cso %t.noprivate.cso

// Strip multiple, verify stripping

// RUN: %dxc %t.private.cso /dumpbin /Qstrip_priv /Qstrip_debug /Qstrip_rootsignature /Fo %t.noprivdebugroot.cso

// RUN: not %dxc %t.noprivdebugroot.cso /dumpbin /getprivate %t.private2.txt

// RUN: not %dxc %t.noprivdebugroot.cso /dumpbin /extractrootsignature %t.rootsig1.cso

// RUN: %dxc %t.noprivdebugroot.cso /dumpbin | FileCheck %s --check-prefix=NO_PRIVATE_DBG_ROOT
// NO_PRIVATE_DBG_ROOT:define void @main()
// NO_PRIVATE_DBG_ROOT-NOT:DICompileUnit

// RUN: %del %t.private.txt
// RUN: %del %t.private.smoke.cso
// RUN: %del %t.noprivate.cso
// RUN: %del %t.private1.txt
// RUN: %del %t.noprivdebugroot.cso
