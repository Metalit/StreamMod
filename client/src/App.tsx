import { Show, createEffect, createSignal, onMount } from "solid-js";
import { Button } from "./components/ui/button";
import "./App.css";
import {
  ColorModeProvider,
  ColorModeScript,
  createLocalStorageManager,
} from "@kobalte/core";
import { Uint8ArrayList, createPersistentSignal } from "./lib/utils";
import {
  Popover,
  PopoverContent,
  PopoverTrigger,
} from "./components/ui/popover";
import { ConnectionIcon, FullscreenIcon } from "./components/ui/icons";
import { Input } from "./ui";
import { OptionsMenu, OptionsProvider, useOptions } from "./Options";
import { SocketProvider, useSocket } from "./Socket";
import { RingBuffer } from "./lib/ringbuf";
import AudioWorkletBuild from "./lib/audio_worklet?worker&url";
import AudioWorkletDev from "./lib/audio_worklet?url";
import DecoderWorker from "./lib/decoder_worker?worker";

class WebAudioController {
  async config(sampleRate: number, channels: number, bufferTime: number) {
    const capacity = sampleRate * channels * bufferTime;

    if (
      this.sampleRate === sampleRate &&
      this.channels === channels &&
      this.buffer?.capacity() === capacity
    )
      return;

    console.log("audio initialize", sampleRate, channels);

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
    return this.play();
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
    await this.audioContext?.resume();
    this.report();
  }

  async pause() {
    clearTimeout(this.reporting);
    return this.audioContext?.suspend();
  }

  async close() {
    await this.pause();
    this.audioSink?.disconnect();
    await this.audioContext?.close();
    this.audioSink = undefined;
    this.audioContext = undefined;
    this.sampleRate = undefined;
    this.channels = undefined;
    this.buffer?.clear();
  }

  sampleRate?: number;
  channels?: number;

  audioContext?: AudioContext;
  audioSink?: AudioWorkletNode;
  buffer?: RingBuffer;

  reportLatency?: (val: number) => void;
  reporting?: NodeJS.Timeout;
}

function VideoDownload() {
  const { ON_MESSAGE, ON_DISCONNECT } = useSocket();

  const file = new Uint8ArrayList();

  let link: HTMLAnchorElement | undefined;

  ON_MESSAGE.addListener((packet) => {
    if (packet.$case === "videoFrame") {
      const { data } = packet.value;
      if (data[4] === 103) file.clear();
      file.add(data);
    }
  });

  ON_DISCONNECT.addListener(() => {
    file.clear();
  });

  const click = () => {
    if (!link) return;
    const blob = new Blob([file.get()]);
    const url = URL.createObjectURL(blob);
    link.href = url;
    link.click();
    // wait 30s to be safe, this is for testing anyway
    setTimeout(() => URL.revokeObjectURL(url), 30000);
  };

  return (
    <Button onClick={click}>
      Download
      <a ref={link} download="video.h264" class="hidden" />
    </Button>
  );
}

const ENABLE_DOWNLOAD_TEST = false;

const decoder = new DecoderWorker();
const audioManager = new WebAudioController();

let canvas: HTMLCanvasElement | undefined;

function Stream() {
  const { width, height, fps } = useOptions();
  const { ON_MESSAGE, ON_DISCONNECT } = useSocket();

  const [latency, setLatency] = createSignal(0);
  audioManager.reportLatency = setLatency;

  createEffect(() => {
    decoder.postMessage({
      type: "latency",
      val: latency(),
    });
  });

  createEffect(() => {
    decoder.postMessage({
      type: "params",
      val: { width: width(), height: height(), fps: fps() },
    });
  });

  onMount(() => {
    const offscreen = canvas!.transferControlToOffscreen();
    decoder.postMessage({ type: "context", val: offscreen }, [offscreen]);
  });

  ON_DISCONNECT.addListener(() => {
    audioManager.close();
    decoder.postMessage({ type: "flush" });
  });

  ON_MESSAGE.addListener((packet) => {
    if (packet.$case === "videoFrame")
      decoder.postMessage({ type: "data", val: packet.value.data });
    else if (packet.$case === "audioFrame") {
      audioManager.config(packet.value.sampleRate, packet.value.channels, 2);
      audioManager.queue(new Float32Array(packet.value.data));
    }
  });

  return (
    <canvas
      ref={canvas}
      width={width()}
      height={height()}
      style={{
        width: `min(100%, calc(100vh - 16px) * ${width()} / ${height()})`,
      }}
    />
  );
}

function ConnectMenu() {
  const { autoConnect, connect, disconnect, connection } = useSocket();

  const [ip, setIp] = createPersistentSignal("connect.address", "192.168.0.1");
  const [port, setPort] = createPersistentSignal("connect.port", "3308");

  const currentConnection = () =>
    connection()?.ip === ip() && connection()?.port === port();

  const submit = () => {
    const reconnect = !currentConnection();
    if (connection()) disconnect();
    if (reconnect) connect(ip(), port());
  };

  onMount(() => {
    if (autoConnect()) submit();
  });

  return (
    <Popover placement="bottom-end">
      <PopoverTrigger as={Button} class="w-10 p-1">
        <ConnectionIcon />
      </PopoverTrigger>
      <PopoverContent class="w-fit flex flex-col gap-4">
        <Input name="Quest IP" value={ip()} onChange={setIp} />
        <Input name="Port" value={port()} onChange={setPort} number />
        <Button onClick={submit}>
          {currentConnection()
            ? "Disconnect"
            : connection()
            ? "Reconnect"
            : "Connect"}
        </Button>
      </PopoverContent>
    </Popover>
  );
}

function MainPage() {
  const { connection } = useSocket();

  return (
    <div id="app" class="flex gap-2 p-2">
      <div class="grow">
        <Show when={connection()}>
          <Stream />
        </Show>
      </div>
      <Show when={ENABLE_DOWNLOAD_TEST}>
        <VideoDownload />
      </Show>
      <div class="flex flex-col gap-2">
        <ConnectMenu />
        <Show when={connection()}>
          <OptionsMenu />
          <Button onClick={() => canvas?.requestFullscreen()} class="w-10 p-1">
            <FullscreenIcon />
          </Button>
        </Show>
      </div>
    </div>
  );
}

function App() {
  const storageManager = createLocalStorageManager("vite-ui-theme");

  return (
    <>
      <ColorModeScript
        storageType={storageManager.type}
        initialColorMode="system"
      />
      <ColorModeProvider storageManager={storageManager}>
        <SocketProvider>
          <OptionsProvider>
            <MainPage />
          </OptionsProvider>
        </SocketProvider>
      </ColorModeProvider>
    </>
  );
}

export default App;
