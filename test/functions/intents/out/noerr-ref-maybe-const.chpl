// infered to be const ref
proc foo(A) {
  return A;
}

// without ref gets infered to be const ref
proc bar(ref A) { // should fail without ref
  A = 1;
}

proc spam(ref A) {
  // as if we had done `with (ref maybe const A)`
  forall i in A.domain {
    A[i] = 2;
  }
}

proc main() {
  var A: [1..10] int;
  A = 1..10;
  writeln("A, ", A);

  var B = foo(A);
  writeln("B, ", B);

  bar(A);
  writeln("A, ", A);

  spam(A);
  writeln("A, ", A);
}
