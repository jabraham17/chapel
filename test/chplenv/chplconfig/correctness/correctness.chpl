/*
   Testing the correcntess of the chplconfig file

   The actual testings takes place in correctness.precomp, this can be
    considered the .good file

 */


writeln("CHPL_TARGET_PLATFORM: darwin +");
writeln("CHPL_TARGET_COMPILER: clang --gcc-toolchain=/usr +");
writeln("CHPL_TARGET_ARCH: x86_64 +");
writeln("CHPL_TARGET_CPU: native +");
writeln("CHPL_LOCALE_MODEL: flat +");
writeln("CHPL_COMM: none +");
writeln("CHPL_TASKS: qthreads +");
writeln("CHPL_LAUNCHER: none +");
writeln("CHPL_TIMERS: generic +");
writeln("CHPL_UNWIND: none +");
writeln("CHPL_TARGET_MEM: jemalloc +");
writeln("CHPL_ATOMICS: intrinsics +");
writeln("CHPL_GMP: none +");
writeln("CHPL_HWLOC: bundled +");
writeln("CHPL_RE2: bundled +");
writeln("CHPL_LLVM: none +");
writeln("CHPL_AUX_FILESYS: none +");
