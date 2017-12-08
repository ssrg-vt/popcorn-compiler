; ModuleID = 'fizzbuzz.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str = private unnamed_addr constant [5 x i8] c"buzz\00"
@str.3 = private unnamed_addr constant [5 x i8] c"fizz\00"
@str.4 = private unnamed_addr constant [9 x i8] c"fizzbuzz\00"

; Function Attrs: nounwind uwtable
define void @fizzbuzz(i32 %max) #0 {
entry:
  call void @check_migrate(void (i8*)* null, i8* null)
  %cmp.23 = icmp eq i32 %max, 0, !popcorn !2
  br i1 %cmp.23, label %for.end, label %for.body.preheader

for.body.preheader:                               ; preds = %entry
  br label %for.body

for.body:                                         ; preds = %for.inc, %for.body.preheader
  %i.024 = phi i32 [ %inc, %for.inc ], [ 0, %for.body.preheader ]
  %rem = urem i32 %i.024, 5
  %rem2 = urem i32 %i.024, 3
  %cmp3 = icmp eq i32 %rem2, 0
  %0 = or i32 %rem2, %rem
  %1 = icmp eq i32 %0, 0
  br i1 %1, label %if.then, label %if.else

if.then:                                          ; preds = %for.body
  %puts22 = tail call i32 @puts(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str.4, i64 0, i64 0))
  br label %for.inc

if.else:                                          ; preds = %for.body
  %cmp1 = icmp eq i32 %rem, 0
  br i1 %cmp1, label %if.then.6, label %if.else.8

if.then.6:                                        ; preds = %if.else
  %puts21 = tail call i32 @puts(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @str.3, i64 0, i64 0))
  br label %for.inc

if.else.8:                                        ; preds = %if.else
  br i1 %cmp3, label %if.then.11, label %for.inc

if.then.11:                                       ; preds = %if.else.8
  %puts = tail call i32 @puts(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @str, i64 0, i64 0))
  br label %for.inc

for.inc:                                          ; preds = %if.then.11, %if.else.8, %if.then.6, %if.then
  %inc = add nuw i32 %i.024, 1
  %exitcond = icmp eq i32 %inc, %max
  br i1 %exitcond, label %for.end.loopexit, label %for.body

for.end.loopexit:                                 ; preds = %for.inc
  br label %for.end

for.end:                                          ; preds = %for.end.loopexit, %entry
  call void @check_migrate(void (i8*)* null, i8* null)
  ret void, !popcorn !2
}

; Function Attrs: nounwind
declare i32 @puts(i8* nocapture readonly) #1

declare void @check_migrate(void (i8*)*, i8*)

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }

!llvm.ident = !{!0}
!llvm.module.flags = !{!1}

!0 = !{!"clang version 3.7.1 (tags/RELEASE_371/final 319349)"}
!1 = !{i32 1, !"popcorn-inst-ty", i32 1}
!2 = !{!"migpoint"}
