define void @test(i1 %cond) {
entry:
  br i1 %cond, label %true_block, label %false_block

true_block:
  br label %merge

false_block:
  br label %merge

merge:
  ret void
}