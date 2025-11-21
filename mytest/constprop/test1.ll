; test1.ll
define i32 @simple_add() {
entry:
  %a = add i32 10, 32        ; 常量相加
  %b = mul i32 %a, 2         ; 链式常量传播
  ret i32 %b                  ; 期望最终为 84
}
