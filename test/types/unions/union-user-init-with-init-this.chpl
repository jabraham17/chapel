union U {
  var x: int;
  var s: string;
  proc init() {
    writeln("initializing");
    init this;
    x = 7;
    writeln("x is ", x);
  }
}

var u:U;
writeln(u.x);
