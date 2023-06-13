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
  proc foo(s: sync int) const ref {
    return s;
  }
  var s: sync int;
  var a = foo(s);
}
else if testnum == 4 {
   proc foo(s: sync int) ref {
    return s;
  }
  var s: sync int;
  var a = foo(s);
}
else if testnum == 5 {
  proc foo(s: single int) { // warn
    return s;
  }
  var s: single int;
  var a = foo(s);
}
else if testnum == 6 {
   proc foo(s: single int) const { // warn
    return s;
  }
  var s: single int;
  var a = foo(s);
}
else if testnum == 7 {
  proc foo(s: single int) const ref {
    return s;
  }
  var s: single int;
  var a = foo(s);
}
else if testnum == 8 {
   proc foo(s: single int) ref {
    return s;
  }
  var s: single int;
  var a = foo(s);
}
