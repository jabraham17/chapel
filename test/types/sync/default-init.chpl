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
    var x: sync int;
    proc init() {}
    proc init=(other: R) {}
  }
  var r = new R();
  class C {
    var x: sync int;
    proc init() {}
  }
  var c = new C();
  union U {
    var x: sync int;
    proc init() {}
    proc init=(other: U) {}
  }
  var u = new U();
}
