/*
 * Copyright 2020-2025 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ChapelHashtable.chpl
//
// This is a low-level hashtable for use by DefaultAssociative.
// The API exposes lots of implementation details.
//
// chpl_TableEntry is the type for each hashtable slot
// chpl__hashtable is the record implementing a hashtable
// hash is the default hash method for most types
pragma "unsafe"
module ChapelHashtable {

  use ChapelBase, DSIUtil;

  private use CTypes, Math, OS.POSIX;

  // empty needs to be 0 so memset 0 sets it
  enum chpl__hash_status { empty=0, full, deleted };

  record chpl_TableEntry {
    var status: chpl__hash_status = chpl__hash_status.empty;
    var key;
    var val;
    inline proc isFull() {
      return this.status == chpl__hash_status.full;
    }
  }

  // ### allocation helpers ###

  // Leaves the elements 0 initialized
  private proc _allocateData(size:int, type tableEltType) {

    if size == 0 then
      halt("attempt to allocate hashtable with size 0");

    var callPostAlloc: bool;
    var ret = _ddata_allocate_noinit(tableEltType,
                                     size,
                                     callPostAlloc);

    var initMethod = init_elts_method(size, tableEltType);

    const sizeofElement = _ddata_sizeof_element(ret);

    // The memset call below needs to be able to set _array records.
    // But c_ptrTo on an _array will return a pointer to
    // the first element, which messes up the shallowCopy/shallowSwap code
    //
    // As a workaround, this function just returns a pointer to the argument,
    // whether or not it is an array.
    inline proc ptrTo(ref x) {
      return c_pointer_return(x);
    }

    select initMethod {
      when ArrayInit.noInit {
        // do nothing
      }
      when ArrayInit.serialInit {
        for slot in _allSlots(size) {
          memset(ptrTo(ret[slot]), 0:uint(8), sizeofElement.safeCast(c_size_t));
        }
      }
      when ArrayInit.parallelInit {
        // This should match the 'these' iterator in terms of idx->task
        forall slot in _allSlots(size) {
          memset(ptrTo(ret[slot]), 0:uint(8), sizeofElement.safeCast(c_size_t));
        }
      }
      when ArrayInit.gpuInit {
        use ChplConfig;
        if CHPL_LOCALE_MODEL=="gpu" {
          extern proc chpl_gpu_memset(addr, byte, numBytes);
          foreach slot in _allSlots(size) {
            chpl_gpu_memset(ptrTo(ret[slot]), 0:uint(8), sizeofElement);
          }
        }
        else {
          halt("ArrayInit.gpuInit should not have been selected");
        }
      }
      otherwise {
        halt("ArrayInit.", initMethod, " should have been implemented");
      }
    }

    if callPostAlloc {
      _ddata_allocate_postalloc(ret, size);
    }

    return ret;
  }

  private proc _freeData(data, size:int) {
    if data != nil {
      _ddata_free(data, size);
    }
  }

  // #### deinit helpers ####
  private proc _typeNeedsDeinit(type t) param {
    return __primitive("needs auto destroy", t);
  }
  private proc _deinitSlot(ref aSlot: chpl_TableEntry) {
    if _typeNeedsDeinit(aSlot.key.type) {
      chpl__autoDestroy(aSlot.key);
    }
    if _typeNeedsDeinit(aSlot.val.type) {
      chpl__autoDestroy(aSlot.val);
    }
  }

  private inline proc _isSlotFull(const ref aSlot: chpl_TableEntry): bool {
    return aSlot.status == chpl__hash_status.full;
  }

  // #### iteration helpers ####

  // Returns the number of chunks to use in parallel iteration
  private proc _allSlotsNumChunks(size: int) {
    const numTasks = if dataParTasksPerLocale==0 then here.maxTaskPar
                     else dataParTasksPerLocale;
    const ignoreRunning = dataParIgnoreRunningTasks;
    const minSizePerTask = dataParMinGranularity;

    // We are simply slicing up the table here.  Trying to do something
    //  more intelligent (like evenly dividing up the full slots), led
    //  to poor speed ups.

    if debugAssocDataPar {
      writeln("### numTasks = ", numTasks);
      writeln("### ignoreRunning = ", ignoreRunning);
      writeln("### minSizePerTask = ", minSizePerTask);
    }

    var numChunks = _computeNumChunks(numTasks, ignoreRunning,
                                      minSizePerTask,
                                      size);

    if debugAssocDataPar {
      writeln("### numChunks=", numChunks, ", size=", size);
    }

    return numChunks;
  }

  // _allSlots yields all slot numbers, empty or full,
  // but does so in the preferred iteration order across tasks.

  iter _allSlots(size: int) {
    for slot in 0..#size {
      yield slot;
    }
  }

  private iter _allSlots(size: int, param tag: iterKind)
    where tag == iterKind.standalone {

    if debugDefaultAssoc {
      writeln("*** In associative domain _allSlots standalone iterator");
    }

    const numChunks = _allSlotsNumChunks(size);

    coforall chunk in 0..#numChunks {
      const (lo, hi) = _computeBlock(size, numChunks, chunk, size-1);
      if debugAssocDataPar then
        writeln("*** chunk: ", chunk, " owns ", lo..hi);
      for slot in lo..hi {
        yield slot;
      }
    }
  }

  private iter _allSlots(size: int, param tag: iterKind)
    where tag == iterKind.leader {

    if debugDefaultAssoc then
      writeln("*** In associative domain _allSlots leader iterator:");

    const numChunks = _allSlotsNumChunks(size);

    coforall chunk in 0..#numChunks {
      const (lo, hi) = _computeBlock(size, numChunks, chunk, size-1);
      if debugDefaultAssoc then
        writeln("*** DI[", chunk, "]: tuple = ", (lo..hi,));
      yield lo..hi;
    }
  }

  private iter _allSlots(size: int, followThis, param tag: iterKind)
    where tag == iterKind.follower {

    var chunk = followThis;

    if debugDefaultAssoc then
      writeln("In associative domain _allSlots follower iterator: ",
              "Following ", chunk);

    foreach slot in chunk {
      yield slot;
    }
  }


  class chpl__rehashHelpers {
    proc startRehash(newSize: int) { }
    proc moveElementDuringRehash(oldSlot: int, newSlot: int) { }
    proc finishRehash(oldSize: int) { }
  }

  record chpl__hashtable {
    type keyType;
    type valType;

    var tableNumFullSlots: int;
    var tableNumDeletedSlots: int;

    var tableSize: int;
    var table: _ddata(chpl_TableEntry(keyType, valType)); // 0..<tableSize

    var rehashHelpers: owned chpl__rehashHelpers?;

    var postponeResize: bool;

    const resizeThreshold: real;

    const startingSize: int;

    proc init(type keyType, type valType, resizeThreshold=defaultHashTableResizeThreshold,
              initialCapacity=16,
              in rehashHelpers: owned chpl__rehashHelpers? = nil) {
      if isDomainType(keyType) then
        compilerError("Values of 'domain' type do not support hash functions yet", 2);
      this.keyType = keyType;
      this.valType = valType;
      this.tableNumFullSlots = 0;
      this.tableNumDeletedSlots = 0;
      this.tableSize = 0;
      this.rehashHelpers = rehashHelpers;
      this.postponeResize = false;
      this.resizeThreshold = resizeThreshold;
      // Round initial capacity up to nearest power of 2
      this.startingSize = 2 << log2((initialCapacity/
                                     resizeThreshold):int-1);
      init this;

      // allocates a _ddata(chpl_TableEntry(keyType,valType)) storing the table
      // All elements are memset to 0 (no initializer is run for the idxType)
      // This allows them to be empty, but the key and val
      // are considered uninitialized.
      this.table = allocateTable(this.tableSize);
    }
    proc deinit() {
      // Go through the full slots in the current table and run
      // chpl__autoDestroy on the index
      if _typeNeedsDeinit(keyType) || _typeNeedsDeinit(valType) {
        if (!_typeNeedsDeinit(keyType) || _deinitElementsIsParallel(keyType, tableSize)) &&
           (!_typeNeedsDeinit(valType) || _deinitElementsIsParallel(valType, tableSize)) {
          forall slot in _allSlots(tableSize) {
            ref aSlot = table[slot];
            if _isSlotFull(aSlot) {
              _deinitSlot(aSlot);
            }
          }
        } else {
          for slot in _allSlots(tableSize) {
            ref aSlot = table[slot];
            if _isSlotFull(aSlot) {
              _deinitSlot(aSlot);
            }
          }
        }
      }

      // Free the buffer
      _freeData(table, tableSize);
    }

    // #### iteration helpers ####

    inline proc isSlotFull(slot: int): bool {
      return table[slot].status == chpl__hash_status.full;
    }

    iter allSlots() {
      foreach slot in _allSlots(tableSize) {
        yield slot;
      }
    }

    iter allSlots(param tag: iterKind)
      where tag == iterKind.standalone {

      foreach slot in _allSlots(tableSize, tag=tag) {
        yield slot;
      }
    }

    iter allSlots(param tag: iterKind)
      where tag == iterKind.leader {

      foreach followThis in _allSlots(tableSize, tag=tag) {
        yield followThis;
      }
    }

    iter allSlots(followThis, param tag: iterKind)
      where tag == iterKind.follower {

      foreach i in _allSlots(tableSize, followThis, tag=tag) {
        yield i;
      }
    }


    // #### add & remove helpers ####

    // a utility to check for key equality
    proc keysMatch(key1: ?t, key2: t) {
      if isArrayType(key2.type) {
        return (key1.equals(key2));
      } else {
        return key1 == key2;
      }
    }

    // Searches for 'key' in a filled slot.
    //
    // Returns (filledSlotFound, slot)
    // filledSlotFound will be true if a matching filled slot was found.
    // slot will be the matching filled slot in that event.
    //
    // If no matching slot was found, slot will store an
    // empty slot that may be re-used for faster addition to the domain
    //
    // This function never returns deleted slots.
    proc _findSlot(key: keyType) : (bool, int) {
      var firstOpen = -1;
      for slotNum in _lookForSlots(key) {
        const slotStatus = table[slotNum].status;
        // if we encounter a slot that's empty, our element could not
        // be found past this point.
        if (slotStatus == chpl__hash_status.empty) {
          if firstOpen == -1 then firstOpen = slotNum;
          return (false, firstOpen);
        } else if (slotStatus == chpl__hash_status.full) {
          if keysMatch(table[slotNum].key, key) {
            return (true, slotNum);
          }
        } else { // this entry was removed, but is the first slot we could use
          if firstOpen == -1 then firstOpen = slotNum;
        }
      }
      return (false, -1);
    }

    iter _lookForSlots(key: keyType, numSlots = tableSize) {
      if numSlots == 0 then return;
      var currentSlot = chpl__defaultHashWrapper(key):uint;
      const mask = numSlots-1;

      foreach probe in 1..numSlots with (ref currentSlot) {
        var uprobe = probe:uint;

        yield (currentSlot&mask):int;
        currentSlot+=uprobe;
      }
    }

    // add pattern:
    //  findAvailableSlot
    //  fillSlot

    // Finds a slot available for adding a key
    // or a slot that was already present with that key.
    // It can rehash the table.
    // returns (foundFullSlot, slotNum)
    proc ref findAvailableSlot(key: keyType): (bool, int) {
      var slotNum = -1;
      var foundSlot = false;

      if ((tableNumFullSlots + tableNumDeletedSlots + 1) *
          (1 / resizeThreshold)):int > tableSize {
        resize(grow=true);
      }

      // Note that when adding elements, if a deleted slot is encountered,
      // later slots need to be checked for the value.
      // That is why this uses the same function that looks for filled slots.
      (foundSlot, slotNum) = _findSlot(key);

      if slotNum >= 0 {
        return (foundSlot, slotNum);
      } else {
        // slotNum < 0
        //
        // This can happen if there are too many deleted elements in the
        // table. In that event, we can garbage collect the table by rehashing
        // everything now.
        rehash(tableSize);

        (foundSlot, slotNum) = _findSlot(key);

        if slotNum < 0 {
          // This shouldn't be possible since we just garbage collected
          // the deleted entries & the table should only ever be half
          // full of non-deleted entries.
          halt("couldn't add key -- ", tableNumFullSlots, " / ", tableSize, " taken");
        }
        return (foundSlot, slotNum);
      }
    }

    proc ref fillSlot(ref tableEntry: chpl_TableEntry(keyType, valType),
                  in key: keyType,
                  in val: valType) {
      use MemMove;

      if tableEntry.status == chpl__hash_status.full {
        _deinitSlot(tableEntry);
      } else {
        if tableEntry.status == chpl__hash_status.deleted {
          tableNumDeletedSlots -= 1;
        }
        tableNumFullSlots += 1;
      }

      tableEntry.status = chpl__hash_status.full;
      // move the key/val into the table
      moveInitialize(tableEntry.key, key);
      moveInitialize(tableEntry.val, val);
    }
    proc ref fillSlot(slotNum: int,
                  in key: keyType,
                  in val: valType) {
      ref tableEntry = table[slotNum];
      fillSlot(tableEntry, key, val);
    }

    // remove pattern:
    //   findFullSlot
    //   clearSlot
    //   maybeShrinkAfterRemove
    //

    // Finds a slot containing a key
    // returns (foundFullSlot, slotNum)
    proc findFullSlot(key: keyType): (bool, int) {
      var slotNum = -1;
      var foundSlot = false;

      (foundSlot, slotNum) = _findSlot(key);

      return (foundSlot, slotNum);
    }

    // Clears a slot that is full
    // (Should not be called on empty/deleted slots)
    // Returns the key and value that were removed in the out arguments
    proc ref clearSlot(ref tableEntry: chpl_TableEntry(keyType, valType),
                   out key: keyType, out val: valType) {
      use MemMove;

      // move the table entry into the key/val variables to be returned
      key = moveFrom(tableEntry.key);
      val = moveFrom(tableEntry.val);

      // set the slot status to deleted
      tableEntry.status = chpl__hash_status.deleted;

      // update the table counts
      tableNumFullSlots -= 1;
      tableNumDeletedSlots += 1;
    }
    proc ref clearSlot(slotNum: int, out key: keyType, out val: valType) {
      // move the table entry into the key/val variables to be returned
      ref tableEntry = table[slotNum];
      clearSlot(tableEntry, key, val);
    }

    proc ref maybeShrinkAfterRemove() {
      // The magic number of 4 was chosen here due to our power of 2
      // table sizes, where shrinking the table means halving the table
      // size, so if your table originally was 1/4 of `resizeThreshold`
      // full, it will be 1/2 of `resizeThreshold` full after the shrink,
      // which seems like a reasonable time to resize
      if (tableSize > startingSize &&
          tableNumFullSlots/tableSize:real < resizeThreshold/4) {
        resize(grow=false);
      }
    }

    // #### rehash / resize helpers ####

    proc _findPowerOf2(numKeys:int) {
      var n = (numKeys - 1): uint;

      var k = 2;

      // TODO: change to `clz()` from BitOps module to avoid while loop
      while n >> 1 > 0 {
        n = n >> 1;
        k = k << 1;
      }
      return k << 1; // shift one more time so capacity is at less than half
    }

    proc allocateData(size: int, type tableEltType) {
      if size == 0 {
        return nil;
      } else {
        return _allocateData(size, tableEltType);
      }
    }
    proc allocateTable(size:int) {
      if size == 0 {
        return nil;
      } else {
        return _allocateData(size, chpl_TableEntry(keyType, valType));
      }
    }

    // newSize is the new table size
    // newSizeNum is an index into chpl__primes == newSize
    // assumes the array is already locked
    proc ref rehash(newSize:int) {
      use MemMove;

      // save the old table
      var oldSize = tableSize;
      var oldTable = table;

      tableSize = newSize;

      var entries = tableNumFullSlots;
      if entries > 0 {
        // There were entries, so carefully move them to the a new allocation

        if newSize == 0 {
          halt("attempt to resize to 0 a table that is not empty");
        }

        table = allocateTable(tableSize);

        if rehashHelpers != nil then
          rehashHelpers!.startRehash(tableSize);

        // tableNumFullSlots stays the same during this operation
        // and all all deleted slots are removed
        tableNumDeletedSlots = 0;

        // Move old data into newly resized table
        //
        // It would be nice if this could be done in parallel
        // but it's possible that multiple old keys will go to the
        // same position in the new array which would lead to data
        // races. So it's not as simple as using forall here.
        for oldslot in _allSlots(oldSize) {
          if oldTable[oldslot].status == chpl__hash_status.full {
            ref oldEntry = oldTable[oldslot];
            // find a destination slot
            var (foundSlot, newslot) = _findSlot(oldEntry.key);
            if foundSlot {
              halt("duplicate element found while resizing for key");
            }
            if newslot < 0 {
              halt("couldn't add element during resize - got slot ", newslot,
                   " for key");
            }

            // move the key and value from the old entry into the new one
            ref dstSlot = table[newslot];
            dstSlot.status = chpl__hash_status.full;
            moveInitialize(dstSlot.key, moveFrom(oldEntry.key));
            moveInitialize(dstSlot.val, moveFrom(oldEntry.val));

            // move array elements to the new location
            if rehashHelpers != nil then
              rehashHelpers!.moveElementDuringRehash(oldslot, newslot);
          }
        }

        if rehashHelpers != nil then
          rehashHelpers!.finishRehash(oldSize);

        // delete the old allocation
        _freeData(oldTable, oldSize);

      } else {
        // There were no entries, so just make a new allocation


        if rehashHelpers != nil {
          rehashHelpers!.startRehash(tableSize);
          rehashHelpers!.finishRehash(oldSize);
        }

        // delete the old allocation
        _freeData(oldTable, oldSize);

        table = allocateTable(tableSize);
        tableNumDeletedSlots = 0;
      }
    }

    proc ref requestCapacity(numKeys:int) {
      if tableNumFullSlots < numKeys {
        rehash(_findPowerOf2(numKeys));
      }
    }

    proc ref resize(grow:bool) {
      if postponeResize then return;

      // double if you are growing, half if you are shrinking
      var newSize = if tableSize == 0 then startingSize else if grow then tableSize << 1 else tableSize >> 1;

      if grow==false && 2*tableNumFullSlots > newSize {
        // don't shrink if the number of elements would not
        // fit into the new size.
        return;
      }

      rehash(newSize);
    }

    iter _evenSlots(param tag) where tag == iterKind.leader {
      for i in (0..<this.tableNumFullSlots).these(tag) {
        yield i;
      }
    }


    iter _evenSlots(followThis, param tag) ref
      where tag == iterKind.follower {

      var space = followThis(0);

      var curNumFull = 0;

      for i in 0..#(this.tableSize) {
        if this.isSlotFull(i) {
          if (curNumFull >= space.low) {
            if (curNumFull <= space.high) {
              yield this.table[i];
            } else {
              break;
            }
          }

          curNumFull += 1;
        }
      }
    }
  }
}
