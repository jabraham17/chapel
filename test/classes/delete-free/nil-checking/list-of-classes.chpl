use List;
class ClassA {
  var x: int;
}
class ClassB {
  var y: int;
}

proc main() {
  var a = new list(owned ClassA);
  var b = new list(owned ClassB);
  writeln(a, " ", b);
  a.pushBack(new ClassA(1));
  b.pushBack(new ClassB(2));
  writeln(a, " ", b);
}
