config param testnum: int = 1;

if testnum == 1 {
  record R {
    var x: sync int;
  }
  var r = new R();
}
else if testnum == 2 {
  class C {
    var x: sync int;
  }
  var c = new C();
}
else if testnum == 3 {
  union U {
    var x: sync int;
  }
  var u = new U();
}
else if testnum == 4 {
  record R {
    var x: single int;
  }
  var r = new R();
}
else if testnum == 5 {
  class C {
    var x: single int;
  }
  var c = new C();
}
else if testnum == 6 {
  union U {
    var x: single int;
  }
  var u = new U();
}
