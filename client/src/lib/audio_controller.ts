import { RingBuffer } from "./ringbuf";
import AudioWorkletBuild from "./audio_worklet?worker&url";
import AudioWorkletDev from "./audio_worklet?url";

export class WebAudioController {
  async config(sampleRate: number, channels: number, bufferTime: number) {
    const capacity = sampleRate * channels * bufferTime;

    if (
      this.sampleRate === sampleRate &&
      this.channels === channels &&
      this.buffer?.capacity() === capacity &&
      this.audioContext
    )
      return;

    console.log("audio initialize", sampleRate, channels, capacity);

    this.sampleRate = sampleRate;
    this.channels = channels;

    if (!this.buffer || this.buffer.capacity() !== capacity) {
      const storage = RingBuffer.getStorageForCapacity(capacity, Float32Array);
      this.buffer = new RingBuffer(storage, Float32Array);
    }

    if (this.audioContext) await this.close();

    // set up the context - requires user to have interacted
    this.audioContext = new AudioContext({
      sampleRate: sampleRate,
      latencyHint: "interactive",
    });
    await this.audioContext.suspend();

    // register the worklet
    // https://github.com/vitejs/vite/issues/6979
    await this.audioContext.audioWorklet.addModule(
      import.meta.env.PROD ? AudioWorkletBuild : AudioWorkletDev
    );

    // "audio_worklet" is the name given to registerProcessor in lib/audio_worklet.ts
    this.audioSink = new AudioWorkletNode(this.audioContext, "audio_worklet", {
      processorOptions: {
        buffer: this.buffer.buf,
      },
      outputChannelCount: [channels],
    });
    this.audioSink.connect(this.audioContext.destination);
    if (!this.paused) return this.play();
    else return this.pause();
  }

  queue(data: Float32Array) {
    this.buffer?.push(data);
  }

  report() {
    const latency =
      this.audioContext!.baseLatency +
      this.audioContext!.outputLatency +
      this.buffer!.available_read() / (this.sampleRate! * this.channels!);
    this.reportLatency?.(latency);
    this.reporting = setTimeout(this.report.bind(this), 500);
  }

  async play() {
    this.paused = false;
    this.buffer?.clear();
    await this.audioContext?.resume();
    if (this.audioContext) this.report();
  }

  async pause() {
    this.paused = true;
    clearTimeout(this.reporting);
    return this.audioContext?.suspend();
  }

  async close() {
    const paused = this.paused;
    await this.pause();
    this.audioSink?.disconnect();
    await this.audioContext?.close();
    this.audioSink = undefined;
    this.audioContext = undefined;
    this.paused = paused;
  }

  async stop() {
    await this.close();
    this.sampleRate = undefined;
    this.channels = undefined;
    this.buffer?.clear();
  }

  sampleRate?: number;
  channels?: number;

  paused = false;

  audioContext?: AudioContext;
  audioSink?: AudioWorkletNode;
  buffer?: RingBuffer;

  reportLatency?: (val: number) => void;
  reporting?: NodeJS.Timeout;
}
