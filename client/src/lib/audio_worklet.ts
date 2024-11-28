import { RingBuffer } from "./ringbuf";

class StreamFeedProcessor extends AudioWorkletProcessor {
  constructor(options: { processorOptions: { buffer: SharedArrayBuffer } }) {
    super();
    this.input = new RingBuffer(options.processorOptions.buffer, Float32Array);
    this.buffer = new Float32Array(this.input.capacity());
    console.log("audio processor initalized");
  }

  input: RingBuffer;
  buffer: Float32Array;

  deinterleave(input: RingBuffer, output: Float32Array[]) {
    const channels = output.length;
    const size = output[0].length * channels;
    const read = input.pop(this.buffer, size);
    let inputIdx = 0;
    for (var i = 0; i < output[0].length && inputIdx < read; i++) {
      for (var j = 0; j < channels; j++) output[j][i] = this.buffer[inputIdx++];
    }
  }

  process(_: Float32Array[][], outputs: Float32Array[][]) {
    this.deinterleave(this.input, outputs[0]);
    return true;
  }
}

registerProcessor("audio_worklet", StreamFeedProcessor);
