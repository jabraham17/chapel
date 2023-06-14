config param testnum: int = 1;
if testnum == 1 {
  proc foo(s: sync int) { // warn
    return s;
  }
  var s: sync int;
  var a = foo(s);
}
else if testnum == 2 {
   proc foo(s: sync int) const { // warn
    return s;
  }
  var s: sync int;
  var a = foo(s);
}
else if testnum == 3 {
  proc foo1(s: sync int) const ref {
    return s;
  }
  var s1: sync int;
  var a1 = foo1(s1);
   proc foo2(s: sync int) ref {
    return s;
  }
  var s2: sync int;
  var a2 = foo2(s2);
}
