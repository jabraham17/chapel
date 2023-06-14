config param testnum: int = 1;
if testnum == 1 {
  proc foo(s: single int) { // warn
    return s;
  }
  var s: single int;
  var a = foo(s);
}
else if testnum == 2 {
   proc foo(s: single int) const { // warn
    return s;
  }
  var s: single int;
  var a = foo(s);
}
else if testnum == 3 {
  proc foo1(s: single int) const ref {
    return s;
  }
  var s1: single int;
  var a1 = foo1(s1);

  proc foo2(s: single int) ref {
    return s;
  }
  var s2: single int;
  var a2 = foo2(s2);
}
