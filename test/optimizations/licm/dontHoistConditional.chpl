// Regression test for https://github.com/chapel-lang/chapel/issues/29034
//
// LICM must not hoist a loop-invariant computation that is only conditionally
// executed within the loop.  Here `maxBytes + 5` is guarded by the
// short-circuiting `&&` (lowered to a conditional), so hoisting it out of the
// loop would make it execute unconditionally and can cause signed integer
// overflow.  In contrast, `maxBytes < max(int)` is executed every iteration and
// may be hoisted.  The prediff inspects the generated C to confirm this.
proc tester(maxBytes, maxChars) {
  var buffSz = 10;
  var n = 0;
  var nChars = 0;
  // CHECK-LABEL: @tester
  // CHECK: entry:
  // CHECK: icmp slt i64 %maxBytes, 9223372036854775807
  // CHECK phi i64
  while nChars < maxChars {
    var requestSz = 2*buffSz;
    // make sure to at least request 16 bytes
    if requestSz < n + 16 then requestSz = n + 16;

    // CHECK: %[[PLUS_5:.+]] = add nsw i64 %maxBytes, 5
    // CHECK: icmp sgt i64 %requestSz{{.*}}, %[[PLUS_5]]

    // but don't ever ask for more bytes than maxBytes + 5
    if maxBytes < max(int) && requestSz > maxBytes + 5 then
      requestSz = maxBytes + 5;
    nChars += requestSz;
  }
  return n;
}
config const maxBytes:int, maxChars:int;
proc main() {
  tester(maxBytes, maxChars);
}
