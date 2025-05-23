//
// A shared module for computing the appropriate problem size for the
// HPCC benchmarks
//
module HPCCProblemSize {
  //
  // Use the standard modules for reasoning about Memory and Types
  //
  use MemDiagnostics, Types;

  //
  // The main routine for computing the problem size
  //
  proc computeProblemSize(numArrays: int,    // #arrays in the benchmark
			 type elemType,     // the element type of those arrays
                         rank=1,            // rank of the arrays
			 returnLog2=false,  // whether to return log2(probSize)
                         memFraction=4,     // fraction of mem to use (eg, 1/4)
                         type retType = int(64)): retType { // type to return

    //
    // Compute the total memory available to the benchmark. If there is one
    // locale per node, then compute the total using a sum reduction over the
    // amount of physical memory (in bytes) owned by the set of locales on
    // which we're running. Otherwise, sum the physical memory of unique
    // nodes as determined by each locale's hostname. Then compute the number
    // of bytes each locale will use as defined by memFraction and the
    // maximum number of co-locales on any node, and the size of each index.
    //

    var totalMem = 0;
    if (max reduce Locales.numColocales > 1) {
      var nodes: domain(string, parSafe=false);
      for loc in Locales {
        if (nodes.contains(loc.hostname) == false) {
          nodes += loc.hostname;
          totalMem += loc.physicalMemory(unit = MemUnits.Bytes);
        }
      }
    } else {
      totalMem = + reduce Locales.physicalMemory(unit = MemUnits.Bytes);
    }

    const memoryTarget = totalMem / memFraction,
      bytesPerIndex = numArrays * numBytes(elemType);

    //
    // Use these values to compute a base number of indices
    //
    var numIndices = memoryTarget / bytesPerIndex;

    //
    // If the user requested a 2**n problem size, compute appropriate
    // values for numIndices and lgProblemSize
    //
    var lgProblemSize = log2(numIndices);
    if (returnLog2) {
      if rank != 1 then 
        halt("computeProblemSize() can't compute 2D 2**n problem sizes yet");
      numIndices = 2**lgProblemSize;
      if (numIndices * bytesPerIndex <= memoryTarget) {
        numIndices *= 2;
        lgProblemSize += 1;
      }
    }

    //
    // Compute the smallest amount of memory that any locale owns
    // using a min reduction and ensure that it is sufficient to hold
    // an even portion of the problem size.
    //
    const smallestMem = min reduce Locales.physicalMemory(unit = MemUnits.Bytes);
    if ((numIndices * bytesPerIndex)/numLocales > smallestMem) then
      halt("System is too heterogeneous: blocked data won't fit into memory");

    //
    // return the problem size as requested by the callee
    //
    if returnLog2 then
      return lgProblemSize: retType;
    else
      select rank {
        when 1 do return numIndices: retType;
        when 2 do return ceil(sqrt(numIndices)): retType;
        otherwise halt("Unexpected rank in computeProblemSize");
      }
  }

  //
  // Print out the machine configuration used to run the job
  //
  proc printLocalesTasks(tasksPerLocale=1) {
    writeln("Number of Locales = ", numLocales);
    writeln("Tasks per locale = ", tasksPerLocale);
  }

  //
  // Print out the problem size, #bytes per array, and total memory
  // required by the arrays
  //
  proc printProblemSize(type elemType, numArrays, problemSize: ?psType, 
                       param rank=1) {
    const bytesPerArray = problemSize**rank * numBytes(elemType),
          totalMemInGB = (numArrays * bytesPerArray:real) / (1024**3),
          lgProbSize = log2(problemSize):psType;

    write("Problem size = ", problemSize);
    for i in 2..rank do write(" x ", problemSize);
    if (2**lgProbSize == problemSize) {
      write(" (2**", lgProbSize);
      for i in 2..rank do write(" x 2**", lgProbSize);
      write(")");
    }
    writeln();
    writeln("Bytes per array = ", bytesPerArray);
    writeln("Total memory required (GB) = ", totalMemInGB);
  }
}
