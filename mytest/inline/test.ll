; test inline 
define i32 @foo(i32 %a, i32 %b) {
entry:
    %sum = add i32 %a, %b
    ret i32 %sum
}

; function with simple if else
define i32 @bar(i32 %p, i32 %q) {
entry:
    %cmp = icmp sgt i32 %p, %q
    br i1 %cmp, label %if.then, label %if.else
if.then:
    %sub = sub i32 %p, %q
    ret i32 %sub
if.else:
    %add = add i32 %p, %q
    ret i32 %add
}

define i32 @main() {
entry:
    %x = call i32 @foo(i32 10, i32 20)
    %y = call i32 @bar(i32 30, i32 15)
    %z = add i32 %x, %y
    ret i32 %z
}