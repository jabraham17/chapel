class A {
  var x;
}

{
  var a = new unmanaged A(7);
  shared.create(a);
}

{
  var a = new A(7);
  shared.create(a);
}

{
  var a = new shared A(7);
  shared.create(a);
}
