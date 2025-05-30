use BlockDist;
use CommDiagnostics;
use MemDiagnostics;
use Random;
use Sort;
use Time;
use ArkoudaRadixSortStandalone;

type elemType = int;

const totMem = here.physicalMemory(unit = MemUnits.Bytes);
config const memFraction = 50;
config const nElemsPerLocale = ((totMem/numBytes(elemType))/memFraction);

config const correctness = false;
config const commCount = false;
config const nElemsTiny = numLocales*4096;
config const nElemsSmall = if correctness then nElemsTiny else 10_000_000;
config const nElemsLarge = numLocales*nElemsPerLocale;

enum arraySize {tiny, small, large};

config const size = if correctness then arraySize.tiny else arraySize.large;

const nElems = if size == arraySize.tiny then nElemsTiny else
               if size == arraySize.small then nElemsSmall else
               if size == arraySize.large then nElemsLarge else -1;

var t: stopwatch;
inline proc startDiag() {
  if !correctness {
    if commCount {
      startCommDiagnostics();
    }
    else {
      t.start();
    }
  }
}

inline proc endDiag(name) {
  if !correctness {
    if commCount {
      stopCommDiagnostics();
      const d = getCommDiagnostics();
      writeln(name, "-GETS: ", + reduce (d.get + d.get_nb));
      writeln(name, "-PUTS: ", + reduce (d.put + d.put_nb));
      writeln(name, "-ONS: ", + reduce (d.execute_on + d.execute_on_fast +
                                        d.execute_on_nb));
      resetCommDiagnostics();
    }
    else {
      t.stop();
      writeln(name, " time : ", t.elapsed());
      const mbPerNode: real = nElems * numBytes(elemType) / (1024*1024) / numLocales;
      writeln(name, " MB/s per node : ", mbPerNode/t.elapsed());
      t.clear();
    }
  }
}

inline proc endDiag(name, x) {
  if correctness {
    if x.size != nElems {
      halt(name , " has unexpected size");
    }
  }
  endDiag(name);
}

proc main() {
  var A = blockDist.createArray({0..<nElems}, elemType);

  fillRandom(A, seed=314159265);
  startDiag();
  TwoArrayDistributedRadixSort.twoArrayDistributedRadixSort(A);
  endDiag("TwoArraySort");
  assert(isSorted(A));

  fillRandom(A, seed=314159265);
  startDiag();
  radixSortLSDCore(A, nBits=numBits(elemType), negs=isIntType(elemType), new KeysComparator());
  endDiag("LSDSort");

  fillRandom(A, seed=314159265);
  var Scratch: [A.domain] elemType;
  var BucketBoundaries: [A.domain] uint(8);
  startDiag();
  Partitioning.psort(A, Scratch, BucketBoundaries,
                     0..<nElems,
                     new defaultComparator(),
                     radixBits=8,
                     logBuckets=8,
                     nTasksPerLocale=PartitioningUtility.computeNumTasks(),
                     endbit=numBits(elemType));
  endDiag("PSort");
}
