config param testnum: int = 1;

if testnum == 1 {
  record R {
    var x: sync int;
  }
}
else if testnum == 2 {
  class C {
    var x: sync int;
  }
}
else if testnum == 3 {
  union C {
    var x: sync int;
  }
}
else if testnum == 4 {
  record R {
    var x: single int;
  }
}
else if testnum == 5 {
  class C {
    var x: single int;
  }
}
else if testnum == 6 {
  union C {
    var x: single int;
  }
}
