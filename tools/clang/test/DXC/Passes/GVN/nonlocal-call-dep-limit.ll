; RUN: %dxopt %s -basicaa -gvn -S | FileCheck %s

; Non-local dependency walks for read-only calls in MemoryDependenceAnalysis
; stop after visiting a limited number of blocks (100), but only once a
; clobbering dependency has been found, since such a result can no longer prove
; the call redundant. Walks that only find Defs are not limited, so GVN still
; removes redundant calls whose dominating definition is far away.

declare i32 @f(i32*) readonly
declare void @g(i32*)

; The dominating call is more than 100 blocks away, but with no clobbers in
; between the limit does not apply and GVN removes the second call.
; CHECK-LABEL: @far_def(
; CHECK: %a = call i32 @f(i32* %p)
; CHECK-NOT: call i32 @f(
; CHECK: add i32 %a, %a
define i32 @far_def(i32* %p) {
entry:
  %a = call i32 @f(i32* %p)
  br label %c0
c0:
  br label %c1
c1:
  br label %c2
c2:
  br label %c3
c3:
  br label %c4
c4:
  br label %c5
c5:
  br label %c6
c6:
  br label %c7
c7:
  br label %c8
c8:
  br label %c9
c9:
  br label %c10
c10:
  br label %c11
c11:
  br label %c12
c12:
  br label %c13
c13:
  br label %c14
c14:
  br label %c15
c15:
  br label %c16
c16:
  br label %c17
c17:
  br label %c18
c18:
  br label %c19
c19:
  br label %c20
c20:
  br label %c21
c21:
  br label %c22
c22:
  br label %c23
c23:
  br label %c24
c24:
  br label %c25
c25:
  br label %c26
c26:
  br label %c27
c27:
  br label %c28
c28:
  br label %c29
c29:
  br label %c30
c30:
  br label %c31
c31:
  br label %c32
c32:
  br label %c33
c33:
  br label %c34
c34:
  br label %c35
c35:
  br label %c36
c36:
  br label %c37
c37:
  br label %c38
c38:
  br label %c39
c39:
  br label %c40
c40:
  br label %c41
c41:
  br label %c42
c42:
  br label %c43
c43:
  br label %c44
c44:
  br label %c45
c45:
  br label %c46
c46:
  br label %c47
c47:
  br label %c48
c48:
  br label %c49
c49:
  br label %c50
c50:
  br label %c51
c51:
  br label %c52
c52:
  br label %c53
c53:
  br label %c54
c54:
  br label %c55
c55:
  br label %c56
c56:
  br label %c57
c57:
  br label %c58
c58:
  br label %c59
c59:
  br label %c60
c60:
  br label %c61
c61:
  br label %c62
c62:
  br label %c63
c63:
  br label %c64
c64:
  br label %c65
c65:
  br label %c66
c66:
  br label %c67
c67:
  br label %c68
c68:
  br label %c69
c69:
  br label %c70
c70:
  br label %c71
c71:
  br label %c72
c72:
  br label %c73
c73:
  br label %c74
c74:
  br label %c75
c75:
  br label %c76
c76:
  br label %c77
c77:
  br label %c78
c78:
  br label %c79
c79:
  br label %c80
c80:
  br label %c81
c81:
  br label %c82
c82:
  br label %c83
c83:
  br label %c84
c84:
  br label %c85
c85:
  br label %c86
c86:
  br label %c87
c87:
  br label %c88
c88:
  br label %c89
c89:
  br label %c90
c90:
  br label %c91
c91:
  br label %c92
c92:
  br label %c93
c93:
  br label %c94
c94:
  br label %c95
c95:
  br label %c96
c96:
  br label %c97
c97:
  br label %c98
c98:
  br label %c99
c99:
  br label %c100
c100:
  br label %c101
c101:
  br label %c102
c102:
  br label %c103
c103:
  br label %c104
c104:
  br label %c105
c105:
  br label %c106
c106:
  br label %c107
c107:
  br label %c108
c108:
  br label %c109
c109:
  br label %use
use:
  %b = call i32 @f(i32* %p)
  %r = add i32 %a, %b
  ret i32 %r
}

; The query in %use finds the clobber in %clob before walking the long path
; through %c0..%c109, so the walk is cut off. The call is not redundant either
; way.
; CHECK-LABEL: @far_def_clobber(
; CHECK: %a = call i32 @f(i32* %p)
; CHECK: call void @g(i32* %p)
; CHECK: %b = call i32 @f(i32* %p)
; CHECK: add i32 %a, %b
define i32 @far_def_clobber(i1 %cond, i32* %p) {
entry:
  %a = call i32 @f(i32* %p)
  br i1 %cond, label %c0, label %clob
clob:
  call void @g(i32* %p)
  br label %use
c0:
  br label %c1
c1:
  br label %c2
c2:
  br label %c3
c3:
  br label %c4
c4:
  br label %c5
c5:
  br label %c6
c6:
  br label %c7
c7:
  br label %c8
c8:
  br label %c9
c9:
  br label %c10
c10:
  br label %c11
c11:
  br label %c12
c12:
  br label %c13
c13:
  br label %c14
c14:
  br label %c15
c15:
  br label %c16
c16:
  br label %c17
c17:
  br label %c18
c18:
  br label %c19
c19:
  br label %c20
c20:
  br label %c21
c21:
  br label %c22
c22:
  br label %c23
c23:
  br label %c24
c24:
  br label %c25
c25:
  br label %c26
c26:
  br label %c27
c27:
  br label %c28
c28:
  br label %c29
c29:
  br label %c30
c30:
  br label %c31
c31:
  br label %c32
c32:
  br label %c33
c33:
  br label %c34
c34:
  br label %c35
c35:
  br label %c36
c36:
  br label %c37
c37:
  br label %c38
c38:
  br label %c39
c39:
  br label %c40
c40:
  br label %c41
c41:
  br label %c42
c42:
  br label %c43
c43:
  br label %c44
c44:
  br label %c45
c45:
  br label %c46
c46:
  br label %c47
c47:
  br label %c48
c48:
  br label %c49
c49:
  br label %c50
c50:
  br label %c51
c51:
  br label %c52
c52:
  br label %c53
c53:
  br label %c54
c54:
  br label %c55
c55:
  br label %c56
c56:
  br label %c57
c57:
  br label %c58
c58:
  br label %c59
c59:
  br label %c60
c60:
  br label %c61
c61:
  br label %c62
c62:
  br label %c63
c63:
  br label %c64
c64:
  br label %c65
c65:
  br label %c66
c66:
  br label %c67
c67:
  br label %c68
c68:
  br label %c69
c69:
  br label %c70
c70:
  br label %c71
c71:
  br label %c72
c72:
  br label %c73
c73:
  br label %c74
c74:
  br label %c75
c75:
  br label %c76
c76:
  br label %c77
c77:
  br label %c78
c78:
  br label %c79
c79:
  br label %c80
c80:
  br label %c81
c81:
  br label %c82
c82:
  br label %c83
c83:
  br label %c84
c84:
  br label %c85
c85:
  br label %c86
c86:
  br label %c87
c87:
  br label %c88
c88:
  br label %c89
c89:
  br label %c90
c90:
  br label %c91
c91:
  br label %c92
c92:
  br label %c93
c93:
  br label %c94
c94:
  br label %c95
c95:
  br label %c96
c96:
  br label %c97
c97:
  br label %c98
c98:
  br label %c99
c99:
  br label %c100
c100:
  br label %c101
c101:
  br label %c102
c102:
  br label %c103
c103:
  br label %c104
c104:
  br label %c105
c105:
  br label %c106
c106:
  br label %c107
c107:
  br label %c108
c108:
  br label %c109
c109:
  br label %use
use:
  %b = call i32 @f(i32* %p)
  %r = add i32 %a, %b
  ret i32 %r
}
