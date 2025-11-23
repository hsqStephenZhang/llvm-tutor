; ModuleID = 'MergeTest'
source_filename = "MergeTest.ll"

define i32 @test(i1 %cond) {
entry:
  br i1 %cond, label %A, label %C

A:                        ; 将要 merge 的块
  %a_val = add i32 10, 0
  br label %B

B:                        ; 被 merge 的块
  %b_val = add i32 %a_val, 1
  br label %D

C:
  %c_val = add i32 100, 0
  br label %D

D:                        ; PHI 节点依赖 B
  %x = phi i32 [ %b_val, %B ], [ %c_val, %C ]
  ret i32 %x
}
