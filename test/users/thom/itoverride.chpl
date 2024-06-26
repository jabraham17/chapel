use IO;

class C : writeSerializable
{
  override proc serialize(writer, ref serializer) throws { writer.write("C"); }
}

class SubC : C, writeSerializable
{
  override proc serialize(writer, ref serializer) throws { writer.write("SubC"); }
}

class OverrideMe
{
  proc getC()
  {
    return new unmanaged C();
  }

  iter manyC(): owned C
  {
    yield new owned C();
    yield new owned C();
  }
}

class OverridesIt : OverrideMe
{
  override proc getC()
  {
    return new unmanaged SubC();
  }

  override iter manyC(): owned C
  {
    yield new owned SubC();
    yield new owned SubC();
  }
}

proc main()
{
  var o : unmanaged OverrideMe = new unmanaged OverridesIt();

  var t1 = o.getC();

  writeln("Should be subC: ", t1);

  delete t1;


  writeln("Many: ", o.manyC());

  delete o;
}
