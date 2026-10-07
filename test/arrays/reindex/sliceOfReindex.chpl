// Slices and reindexes of reindex views, including chains, writes through
// them, strided arrays, and multiple dimensions.

proc show(x) {
  writeln(x, " ", x.domain);
}

proc sameViewType(a, b) param do return a._value.type == b._value.type;

proc sumRest(x): int {
  if x.size == 0 then return 0;
  return x[x.domain.low] + sumRest(x[x.domain.low+1..].reindex(0..#x.size-1));
}

{
  var A: [1..10] int = 1..10;
  show(A[3..8].reindex(0..5));
  show(A[3..8].reindex(0..5)[2..4]);
  show(A[3..8].reindex(0..5)[2..4].reindex(1..3)[2..3]);
  writeln(A[3..8].reindex(0..5)[2..4].reindex(1..3)[3]);
  show(A.reindex(0..9)[1..]);
  show(A.reindex(0..9)[{2..4}]);
  show(A.reindex(0..18 by 2)[4..10]);
  show(A.reindex(0..18 by 2)[4..10].reindex(1..4)[2..3]);
  writeln(sumRest(A));
  writeln(sameViewType(A[1..].reindex(0..9),
                       A[1..].reindex(0..9)[1..].reindex(0..8)));

  A.reindex(0..9)[4..6] = [100, 200, 300];
  A[2..9].reindex(1..8)[2..3].reindex(5..6)[6] = -1;
  writeln(A);
  var B: [1..3] int = A.reindex(0..9)[4..6];
  writeln(B);
}

{
  var S: [1..20 by 2] int;
  for i in S.domain do S[i] = i;
  show(S.reindex(0..9)[2..5]);
  show(S.reindex(0..9)[2..5].reindex(1..4)[3..4]);
  show(S.reindex(0..9)[0..9 by 3]);
  show(S.reindex(0..9)[0..9 by 3].reindex(1..4)[2..3]);
  S.reindex(0..9)[2..5].reindex(1..4)[3..4] = -1;
  writeln(S);
  writeln(sumRest(S));
}

{
  var M: [1..3, 1..4] int;
  for (i, j) in M.domain do M[i, j] = i*10 + j;
  writeln(M.reindex(0..2, 0..3)[1..2, 1..3]);
  writeln(M.reindex(0..2, 0..3)[1..2, 1..3].domain);
  show(M.reindex(0..2, 0..3)[1..2, 1..3].reindex(0..1, 0..2)[1..1, 1..2]);
  M.reindex(0..2, 0..3)[1..2, 1..3].reindex(0..1, 0..2)[0, 0] = 0;
  writeln(M);
}
