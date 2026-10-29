// The static 'helper' from the header must not be replaced by the global
// 'helper' in the C file, even when the module is split with '-j'.
require "staticHeaderClash.h", "staticHeaderClashOther.c";

use CTypes;

extern proc helper(x: c_int): c_int;

config const x: c_int = 41;
writeln(helper(x));
