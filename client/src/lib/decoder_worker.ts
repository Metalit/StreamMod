let decoderError = false;
let wroteSps = false;
let spsPacket: Uint8Array | undefined;
let canvas: OffscreenCanvas | undefined;
let canvasContext: OffscreenCanvasRenderingContext2D | undefined;
let decoder: VideoDecoder | undefined;

const isChrome = /chrom(e|ium)/.test(navigator.userAgent.toLowerCase());

const config: VideoDecoderConfig = {
  codec: "avc1.42000a",
  optimizeForLatency: true,
  hardwareAcceleration: "prefer-hardware",
};

const isKey = (data: Uint8Array) => data[4] === 101 || data[4] === 103;

const init: VideoDecoderInit = {
  output: (frame) => {
    canvasContext?.drawImage(frame, 0, 0);
    frame.close();
  },
  error: (error) => {
    console.error("decoder error", error);
    decoderError = true;
  },
};

const frameBuffer: Uint8Array[] = [];

let targetLatency = 0;
let currentFps = 0;

const atLatency = () => {
  if (!decoder || frameBuffer.length === 0 || currentFps < 1) return false;
  return (
    (decoder.decodeQueueSize + frameBuffer.length) / currentFps >= targetLatency
  );
};

const feed = () => {
  if (!atLatency()) return;
  // todo: skip to the next key frame if that frame is also behind the latency
  const data = frameBuffer.shift()!;
  const chunk = new EncodedVideoChunk({
    data,
    timestamp: 0,
    type: isKey(data) ? "key" : "delta",
  });
  decoder!.decode(chunk);
};

const queue = (data: Uint8Array) => {
  frameBuffer.push(data);
  feed();
};

VideoDecoder.isConfigSupported(config).then((support) => {
  if (!support.supported) {
    console.error("config unsupported!", support);
    return;
  }
  decoder = new VideoDecoder(init);
  decoder.configure(config);
  decoder.ondequeue = feed;
});

const flush = () => {
  // clear queue
  frameBuffer.length = 0;
  // restart decoder
  spsPacket = undefined;
  wroteSps = false;
  decoder?.reset();
  decoder?.configure(config);
};

self.onmessage = (event: MessageEvent) => {
  const data = event.data as
    | { type: "context"; val: OffscreenCanvas }
    | { type: "params"; val: { width: number; height: number; fps: number } }
    | { type: "latency"; val: number }
    | { type: "flush" }
    | { type: "data"; val: Uint8Array };

  switch (data.type) {
    case "context":
      canvas = data.val;
      canvasContext = canvas.getContext("2d")!;
      break;
    case "params":
      currentFps = data.val.fps;
      if (canvas) {
        canvas.width = data.val.width;
        canvas.height = data.val.height;
        canvasContext = canvas.getContext("2d")!;
        flush();
      }
      break;
    case "latency":
      targetLatency = data.val;
      break;
    case "flush":
      flush();
      break;
    default:
      try {
        if (decoder === undefined || decoderError) return;
        let array = data.val;
        if (
          array[0] !== 0 ||
          array[1] !== 0 ||
          array[2] !== 0 ||
          array[3] !== 1
        )
          console.warn("invalid header", array);
        // firefox takes sps as a separate packet, chrome wants it stuck together with a keyframe
        if (isChrome && array[4] === 103) {
          spsPacket = new Uint8Array(array);
          return;
        }
        if (spsPacket !== undefined && !wroteSps && array[4] === 101) {
          console.log("combining sps with normal packet");
          array = new Uint8Array([...spsPacket!, ...array]);
          wroteSps = true;
        }
        if (!wroteSps && array[4] !== 103) return;
        queue(array);
      } catch (error) {
        console.error("decoding error", error);
      }
      break;
  }
};
