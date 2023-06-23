proc t1(const tup) {
  tup(0)[0] = 2;
  writeln(tup(1)[0]);
}
proc main() {
  var A = [1,2,3,4];
  var t = (A,A);
  writeln(t);
  t1(t);
  writeln(t);
}
