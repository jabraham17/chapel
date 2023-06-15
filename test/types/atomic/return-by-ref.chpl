config param testnum: int = 1;
if testnum == 1 {
  proc foo(s: atomic int) { // warn
    return s;
  }
  var s: atomic int;
  var a = foo(s);
}
else if testnum == 2 {
   proc foo(s: atomic int) const { // warn
    return s;
  }
  var s: atomic int;
  var a = foo(s);
}
else if testnum == 3 {
  proc foo1(s: atomic int) const ref {
    return s;
  }
  var s1: atomic int;
  var a1 = foo1(s1);
   proc foo2(s: atomic int) ref {
    return s;
  }
  var s2: atomic int;
  var a2 = foo2(s2);
}
