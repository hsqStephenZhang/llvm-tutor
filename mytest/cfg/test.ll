define i32 @f() {
entry:
  br label %A

A:
  br label %B

B:
  ret i32 42
}

define i32 @g() {
entry:
  br label %A

A:
  br label %B

B:
  br label %C

C:
  ret i32 7
}

define i32 @h(i1 %cond) {
entry:
  br i1 %cond, label %A, label %B

A:
  br label %C

B:
  br label %C

C:
  %x = phi i32 [1, %A], [2, %B]
  ret i32 %x
}

define void @i() {
entry:
  br label %A

A:
  call void @llvm.dbg.value(metadata i32 1, metadata !0, metadata !1)
  br label %B

B:
  ret void
}

declare void @llvm.dbg.value(metadata, metadata, metadata)

!0 = !{!"var"}
!1 = !{!"debug info"}
