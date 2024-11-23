import { createEffect, onMount } from "solid-js";
import { Button } from "./components/ui/button";
import "./App.css";
import {
  ColorModeProvider,
  ColorModeScript,
  createLocalStorageManager,
} from "@kobalte/core";
import { createPersistentSignal } from "./lib/utils";
import {
  Popover,
  PopoverContent,
  PopoverTrigger,
} from "./components/ui/popover";
import { ConnectionIcon } from "./components/ui/icons";
import { Input } from "./ui";
import { OptionsMenu, OptionsProvider, useOptions } from "./Options";
import { SocketProvider, useSocket } from "./Socket";
import { RingBuffer } from "./lib/ringbuf";

class WebAudioController {
  async config(sampleRate: number, channels: number) {
    if (this.sampleRate === sampleRate && this.channels === channels) return;

    console.log("audio initialize", sampleRate, channels);

    this.sampleRate = sampleRate;
    this.channels = channels;

    // Set up AudioContext to house graph of AudioNodes and control rendering.
    this.audioContext = new AudioContext({
      sampleRate: sampleRate,
      latencyHint: "interactive",
    });
    this.audioContext.suspend();

    // Make script modules available for execution by AudioWorklet.
    await this.audioContext.audioWorklet.addModule(
      new URL("lib/audio_worklet.ts", import.meta.url)
    );

    // discard data after 0.2 seconds
    const storage = RingBuffer.getStorageForCapacity(
      sampleRate * channels * 0.2,
      Float32Array
    );
    this.buffer = new RingBuffer(storage, Float32Array);

    // Get an instance of the AudioSink worklet, passing it the memory for a
    // ringbuffer, connect it to a GainNode for volume. This GainNode is in
    // turn connected to the destination.
    this.audioSink = new AudioWorkletNode(this.audioContext, "audio_worklet", {
      processorOptions: {
        buffer: storage,
      },
      outputChannelCount: [channels],
    });
    this.audioSink.connect(this.audioContext.destination);
    return this.audioContext.resume();
  }

  queue(data: Float32Array) {
    this.buffer?.push(data);
  }

  async play() {
    return this.audioContext.resume();
  }

  async pause() {
    return this.audioContext.suspend();
  }

  sampleRate?: number;
  channels?: number;

  audioContext!: AudioContext;
  audioSink!: AudioWorkletNode;
  buffer?: RingBuffer;
}

function Stream() {
  const { width, height } = useOptions();
  const { ON_MESSAGE, ON_CONNECT, ON_DISCONNECT } = useSocket();

  const decoder = new Worker(new URL("lib/decoder_worker.ts", import.meta.url));

  let canvas: HTMLCanvasElement | undefined;

  const audioManager = new WebAudioController();

  createEffect(() => {
    decoder.postMessage({
      type: "dimensions",
      val: { width: width(), height: height() },
    });
  });

  onMount(() => {
    const offscreen = canvas!.transferControlToOffscreen();
    decoder.postMessage({ type: "context", val: offscreen }, [offscreen]);
  });

  ON_CONNECT.addListener(() => {});

  ON_DISCONNECT.addListener(() => {
    audioManager.pause();
  });

  ON_MESSAGE.addListener((packet) => {
    if (packet.$case === "videoFrame")
      decoder.postMessage({ type: "data", val: packet.value.data });
    else if (packet.$case === "audioFrame") {
      audioManager.config(packet.value.sampleRate, packet.value.channels);
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
            <div id="app" class="flex gap-2 p-2">
              <div class="grow">
                {/* <Show when={connection()}>
              <Stream width={width()} height={height()} />
            </Show> */}
                <Stream />
              </div>
              <OptionsMenu />
              <ConnectMenu />
            </div>
          </OptionsProvider>
        </SocketProvider>
      </ColorModeProvider>
    </>
  );
}

export default App;
