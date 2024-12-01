// A Single Producer - Single Consumer thread-safe wait-free ring buffer.
//
// The producer and the consumer can be on separate threads, but cannot change roles,
// except with external synchronization.

type TypedArray =
  | typeof Int8Array
  | typeof Uint8Array
  | typeof Uint8ClampedArray
  | typeof Int16Array
  | typeof Uint16Array
  | typeof Int32Array
  | typeof Uint32Array
  | typeof Float32Array
  | typeof Float64Array
  | typeof BigInt64Array
  | typeof BigUint64Array;

type Instance = InstanceType<TypedArray>;

export class RingBuffer {
  _type: TypedArray;
  _capacity: number;
  buf: SharedArrayBuffer;
  write_ptr: Uint32Array;
  read_ptr: Uint32Array;
  storage: Instance;

  static getStorageForCapacity(capacity: number, type: TypedArray) {
    var bytes = 8 + (capacity + 1) * type.BYTES_PER_ELEMENT;
    return new SharedArrayBuffer(bytes);
  }
  // `sab` is a SharedArrayBuffer with a capacity calculated by calling
  // `getStorageForCapacity` with the desired capacity.
  constructor(sab: SharedArrayBuffer, type: TypedArray) {
    // Maximum usable size is 1<<32 - type.BYTES_PER_ELEMENT bytes in the ring
    // buffer for this version, easily changeable.
    // -4 for the write ptr (uint32_t offsets)
    // -4 for the read ptr (uint32_t offsets)
    // capacity counts the empty slot to distinguish between full and empty.
    this._type = type;
    this._capacity = (sab.byteLength - 8) / type.BYTES_PER_ELEMENT;
    this.buf = sab;
    this.write_ptr = new Uint32Array(this.buf, 0, 1);
    this.read_ptr = new Uint32Array(this.buf, 4, 1);
    this.storage = new type(this.buf, 8, this._capacity);
  }

  // Push bytes to the ring buffer. `elements` is a typed array of the same type
  // as passed in the ctor, to be written to the queue.
  // Returns the number of elements written to the queue.
  push(elements: Instance) {
    const rd = Atomics.load(this.read_ptr, 0);
    const wr = Atomics.load(this.write_ptr, 0);

    // full
    if ((wr + 1) % this._storage_capacity() == rd) return 0;

    const to_write = Math.min(this._available_write(rd, wr), elements.length);
    const first_part = Math.min(this._storage_capacity() - wr, to_write);
    const second_part = to_write - first_part;

    this._copy(elements, 0, this.storage, wr, first_part);
    this._copy(elements, first_part, this.storage, 0, second_part);

    // publish the enqueued data to the other side
    Atomics.store(
      this.write_ptr,
      0,
      (wr + to_write) % this._storage_capacity()
    );

    return to_write;
  }

  // Read `elements.length` elements from the ring buffer. `elements` is a typed
  // array of the same type as passed in the ctor.
  // Returns the number of elements read from the queue, they are placed at the
  // beginning of the array passed as parameter.
  pop(elements: Instance, length?: number) {
    const rd = Atomics.load(this.read_ptr, 0);
    const wr = Atomics.load(this.write_ptr, 0);

    if (wr == rd) return 0;

    const to_read = Math.min(
      this._available_read(rd, wr),
      length ?? elements.length
    );

    const first_part = Math.min(this._storage_capacity() - rd, to_read);
    const second_part = to_read - first_part;

    this._copy(this.storage, rd, elements, 0, first_part);
    this._copy(this.storage, 0, elements, first_part, second_part);

    Atomics.store(this.read_ptr, 0, (rd + to_read) % this._storage_capacity());

    return to_read;
  }

  clear() {
    const wr = Atomics.load(this.write_ptr, 0);
    Atomics.store(this.read_ptr, 0, wr);
  }

  // True if the ring buffer is empty false otherwise. This can be late on the
  // reader side: it can return true even if something has just been pushed.
  empty() {
    const rd = Atomics.load(this.read_ptr, 0);
    const wr = Atomics.load(this.write_ptr, 0);

    return wr == rd;
  }

  // True if the ring buffer is full, false otherwise. This can be late on the
  // write side: it can return true when something has just been popped.
  full() {
    const rd = Atomics.load(this.read_ptr, 0);
    const wr = Atomics.load(this.write_ptr, 0);

    return (wr + 1) % this._storage_capacity() == rd;
  }

  // The usable capacity for the ring buffer: the number of elements that can be
  // stored.
  capacity() {
    return this._capacity - 1;
  }

  // Number of elements available for reading. This can be late, and report less
  // elements that is actually in the queue, when something has just been
  // enqueued.
  available_read() {
    const rd = Atomics.load(this.read_ptr, 0);
    const wr = Atomics.load(this.write_ptr, 0);
    return this._available_read(rd, wr);
  }

  // Number of elements available for writing. This can be late, and report less
  // elements that is actually available for writing, when something has just
  // been dequeued.
  available_write() {
    const rd = Atomics.load(this.read_ptr, 0);
    const wr = Atomics.load(this.write_ptr, 0);
    return this._available_write(rd, wr);
  }

  // private methods //

  // Number of elements available for reading, given a read and write pointer..
  _available_read(rd: number, wr: number) {
    return (wr + this._storage_capacity() - rd) % this._storage_capacity();
  }

  // Number of elements available from writing, given a read and write pointer.
  _available_write(rd: number, wr: number) {
    return this.capacity() - this._available_read(rd, wr);
  }

  // The size of the storage for elements not accounting the space for the
  // index, counting the empty slot.
  _storage_capacity() {
    return this._capacity;
  }

  // Copy `size` elements from `input`, starting at offset `offset_input`, to
  // `output`, starting at offset `offset_output`.
  _copy(
    input: Instance,
    offset_input: number,
    output: Instance,
    offset_output: number,
    size: number
  ) {
    for (var i = 0; i < size; i++)
      output[offset_output + i] = input[offset_input + i];
  }
}
