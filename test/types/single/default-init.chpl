config param testnum: int = 1;

if testnum == 1 {
  record R {
    var x: single int;
  }
  var r = new R();
}
else if testnum == 2 {
  class C {
    var x: single int;
  }
  var c = new C();
}
else if testnum == 3 {
  union U {
    var x: single int;
  }
  var u = new U();
}
