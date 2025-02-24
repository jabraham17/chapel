use EpochManager;
use CyclicDist;
use Random;

class C {
  var x : int;

  proc deinit() {
    writeln("Reclaimed.");
  }
}

proc main() {
  // Create manager instance
  var manager = new EpochManager();
  const numObjects = 16;
  var objsDom = {0..#numObjects} dmapped new cyclicDist(startIdx=0);
  var objs : [objsDom] unmanaged C?;

  // Scattering objects around the locales
  forall obj in objs with (var rng = new randomStream(int)) {
    on Locales[abs(rng.next()) % numLocales] do obj = new unmanaged C(0);
  }

  // Registering tasks with the manager
  forall obj in objs with (var tok = manager.register()) {
    // pin the task
    tok.pin();

    // delete obj
    tok.deferDelete(obj);

    // unpin the task
    tok.unpin();
  }

  // Reclaim
  manager.clear();
  
  manager.destroy();
}
