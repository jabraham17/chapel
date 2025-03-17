use IO, ChplFormat;

var A: [1..2] string;
var B: [1..2, 1..2] string;

var infile = open("readDifferentRankArrays.dat", ioMode.r);
var f = infile.reader(deserializer = new chplDeserializer(), locking=false);

f.readf("%?", A);
writeln(A);

writeln("======");

f.readf("%?", B);
writeln(B);
