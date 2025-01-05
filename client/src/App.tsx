import { Show, createEffect, createSignal, onCleanup, onMount } from "solid-js";
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
import { Input as ProtoInput } from "./proto/stream";

class WebAudioController {
  async config(sampleRate: number, channels: number, bufferTime: number) {
    const capacity = sampleRate * channels * bufferTime;

    if (
      this.sampleRate === sampleRate &&
      this.channels === channels &&
      this.buffer?.capacity() === capacity
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
  }

  async stop() {
    await this.close();
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

class InputManager {
  loop() {
    if (Object.keys(this.next).length !== 0) {
      this.send?.(this.next);
      this.next = {};
    }
    this.looping = setTimeout(this.loop.bind(this), this.timeout);
    // this.looping = requestAnimationFrame(this.send.bind(this));
  }

  start() {
    if (!this.looping) this.loop();
  }

  stop() {
    if (this.looping) clearTimeout(this.looping);
    // if (this.looping) cancelAnimationFrame(this.looping);
    this.looping = undefined;
  }

  onMouseMove(event: MouseEvent) {
    this.next.dx = (this.next.dx ?? 0) + event.movementX;
    this.next.dy = (this.next.dy ?? 0) - event.movementY;
  }

  onMouseDown(event: MouseEvent) {
    if (event.button === 0) this.next.mouseDown = true;
  }

  onMouseUp(event: MouseEvent) {
    if (event.button === 0) this.next.mouseUp = true;
  }

  onKeyDown(event: KeyboardEvent) {
    this.next.keysDown = [...(this.next.keysDown ?? []), event.key];
  }

  onKeyUp(event: KeyboardEvent) {
    this.next.keysUp = [...(this.next.keysUp ?? []), event.key];
  }

  onWheel(event: WheelEvent) {
    this.next.scroll = (this.next.scroll ?? 0) - event.deltaY / 50;
  }

  bind() {
    this.unbind?.();

    const onMouseMove = this.onMouseMove.bind(this);
    const onMouseDown = this.onMouseDown.bind(this);
    const onMouseUp = this.onMouseUp.bind(this);
    const onKeyDown = this.onKeyDown.bind(this);
    const onKeyUp = this.onKeyUp.bind(this);
    const onWheel = this.onWheel.bind(this);

    document.addEventListener("mousemove", onMouseMove);
    document.addEventListener("mousedown", onMouseDown);
    document.addEventListener("mouseup", onMouseUp);
    document.addEventListener("keydown", onKeyDown);
    document.addEventListener("keyup", onKeyUp);
    document.addEventListener("wheel", onWheel);

    this.unbind = () => {
      document.removeEventListener("mousemove", onMouseMove);
      document.removeEventListener("mousedown", onMouseDown);
      document.removeEventListener("mouseup", onMouseUp);
      document.removeEventListener("keydown", onKeyDown);
      document.removeEventListener("keyup", onKeyUp);
      document.removeEventListener("wheel", onWheel);
      this.unbind = undefined;
      this.stop();
    };

    this.start();
  }

  unbind?: () => void;

  next: Partial<ProtoInput> = {};
  looping?: NodeJS.Timeout;
  // looping?: number;

  timeout = 1000 / 60;
  send?: (value: Partial<ProtoInput>) => void;
}

const ENABLE_DOWNLOAD_TEST = false;

const decoder = new DecoderWorker();
const audioManager = new WebAudioController();
const inputManager = new InputManager();

let canvas: HTMLCanvasElement | undefined;

function Stream() {
  const { width, height, fps, fpfc } = useOptions();
  const { ON_MESSAGE, ON_DISCONNECT, send } = useSocket();

  const [latency, setLatency] = createSignal(0);
  audioManager.reportLatency = setLatency;

  const onLockChange = () => {
    const locked =
      canvas !== undefined && document.pointerLockElement === canvas;
    console.log("pointer lock", locked);
    if (locked) inputManager.bind();
    else inputManager.unbind?.();
  };

  onMount(() => {
    document.addEventListener("pointerlockchange", onLockChange);
    inputManager.send = (value: Partial<ProtoInput>) =>
      send({ $case: "input", value });
  });

  onCleanup(() => {
    document.removeEventListener("pointerlockchange", onLockChange);
    inputManager.unbind?.();
  });

  createEffect(() => {
    decoder.postMessage({
      type: "latency",
      val: fpfc() ? 0 : latency(),
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
      audioManager.config(
        packet.value.sampleRate,
        packet.value.channels,
        fpfc() ? 0.2 : 2
      );
      audioManager.queue(new Float32Array(packet.value.data));
    }
  });

  return (
    <canvas
      ref={canvas}
      onClick={() => {
        // will throw if you click again too quickly. TODO: set timeout and try again
        if (fpfc()) canvas?.requestPointerLock().catch(() => {});
      }}
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
