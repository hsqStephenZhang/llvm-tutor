; Case 1: 最基础的测试
; 预期：删除 %dead_1, %dead_2
define i32 @basic_test(i32 %arg) {
entry:
  %dead_1 = add i32 %arg, 10     ; [DEAD] 没有任何人用它
  %alive_1 = mul i32 %arg, 2     ; [ALIVE] 被返回值使用
  %dead_2 = sub i32 %alive_1, 5  ; [DEAD] 虽然用了 alive_1，但结果没人用
  ret i32 %alive_1
}

; Case 2: 链式反应 (Chain Reaction)
; 测试你的算法是否使用了 Worklist 或递归。
; 删除 %unused_3 后，%unused_2 引用计数归零，也应被删，依此类推。
; 预期：只保留 ret void
define void @chain_reaction(i32 %a, i32 %b) {
entry:
  %unused_1 = add i32 %a, %b        ; [DEAD] 只有 unused_2 用它
  %unused_2 = mul i32 %unused_1, 5  ; [DEAD] 只有 unused_3 用它
  %unused_3 = sub i32 %unused_2, 1  ; [DEAD] 没人用它，它是删除链的起点
  ret void
}

; Case 3: 跨 Basic Block 的无用指令
; 预期：删除 %dead_in_entry, %dead_in_true
define i32 @cross_block_test(i1 %cond) {
entry:
  %dead_in_entry = add i32 1, 1       ; [DEAD]
  br i1 %cond, label %if.true, label %if.end

if.true:
  %dead_in_true = add i32 2, 2        ; [DEAD]
  %alive_in_true = add i32 3, 3       ; [ALIVE] 被 PHI 节点使用
  br label %if.end

if.end:
  %result = phi i32 [ 0, %entry ], [ %alive_in_true, %if.true ]
  ret i32 %result
}