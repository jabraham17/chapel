use IO;
use List;

record R {
  var x: int = 17;
}

var strings = [
  "%10?",
  "%10.1?",
  "%<10.1?",
  "%>10.2?",
  "%j?",
  "%o?",
  "%h?",
  "%e?",
  "%@?",
  "%+?",
  "%~?",
  "%^?",
  "%0?",
  "% ?",
];

stdout.writeln("formatting ints");
for s in strings {
  try {
    stdout.write("format string '", s, "': ");
    stdout.writef(s, 17);
    stdout.writeln();
  } catch e {
    stdout.writeln(e);
  }
}
stdout.writeln();
stdout.writeln("formatting floats");
for s in strings {
  try {
    stdout.write("format string: '", s, "': ");
    stdout.writef(s, 17.0);
    stdout.writeln();
  } catch e {
    stdout.writeln(e);
  }
}
stdout.writeln();
stdout.writeln("formatting records");
for s in strings {
  try {
    stdout.write("format string: '", s, "': ");
    stdout.writef(s, new R());
    stdout.writeln();
  } catch e {
    stdout.writeln(e);
  }
}
