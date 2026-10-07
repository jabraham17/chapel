// Slices and reindexes of reindex views over a Block-distributed array.
use BlockDist;

proc show(x) {
  writeln(x, " ", x.domain);
}

proc sumRest(x): int {
  if x.size == 0 then return 0;
  return x[x.domain.low] + sumRest(x[x.domain.low+1..].reindex(0..#x.size-1));
}

const D = blockDist.createDomain({1..10});
var A: [D] int = 1..10;

show(A[3..8].reindex(0..5)[2..4]);
show(A[3..8].reindex(0..5)[2..4].reindex(1..3)[2..3]);
show(A.reindex(0..9)[{2..4}]);
writeln(sumRest(A));

A.reindex(0..9)[4..6] = [100, 200, 300];
A[2..9].reindex(1..8)[2..3].reindex(5..6)[6] = -1;
forall a in A.reindex(0..9)[7..9] do a *= 2;
writeln(A);

var B: [1..3] int = A.reindex(0..9)[4..6];
writeln(B);
