config param testnum = 1;
config param modify = false;
proc main()
{
  var xs : [0 ..# 3] int = [1, 2, 3];
  if testnum == 1 then
    coforall loc in Locales do on loc {
      writeln(xs);
      if modify then xs += 1;
    }
  if testnum == 2 then
    coforall loc in Locales with (const xs) do on loc {
      writeln(xs);
      if modify then xs += 1;
    }
  if testnum == 3 then
    coforall loc in Locales with (const in xs) do on loc {
      writeln(xs);
      if modify then xs += 1;
    }
  if testnum == 4 then
    coforall loc in Locales with (const ref xs) do on loc {
      writeln(xs);
      if modify then xs += 1;
    }
  if testnum == 5 then
    coforall loc in Locales with (in xs) do on loc {
      writeln(xs);
      if modify then xs += 1;
    }
  if testnum == 6 then
    coforall loc in Locales with (ref xs) do on loc {
      writeln(xs);
      if modify then xs += 1;
    }
}
