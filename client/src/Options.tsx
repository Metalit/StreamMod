import {
  batch,
  createContext,
  createEffect,
  createSignal,
  ParentProps,
  Show,
  useContext,
} from "solid-js";
import { Button } from "./components/ui/button";
import { OptionsIcon } from "./components/ui/icons";
import {
  Popover,
  PopoverContent,
  PopoverTrigger,
} from "./components/ui/popover";
import { Separator } from "./components/ui/separator";
import {
  Switch,
  SwitchControl,
  SwitchLabel,
  SwitchThumb,
} from "./components/ui/switch";
import { Select, Slider, Spinner } from "./ui";
import { useSocket } from "./Socket";

const resolutions = {
  "720p": [1280, 720],
  "1080p": [1920, 1080],
  "1440p": [2560, 1440],
} as const;

const micMixModes = ["Combine", "Duck", "Add"];

function makeOptionsContext() {
  const { send, ON_DISCONNECT, ON_MESSAGE } = useSocket();

  // default aspect ratio
  const [width, setWidth] = createSignal(16);
  const [height, setHeight] = createSignal(9);
  const [bitrate, setBitrate] = createSignal(0);
  const [fps, setFps] = createSignal(0);
  const [fov, setFov] = createSignal(0);
  const [smoothness, setSmoothness] = createSignal(0);
  const [mic, setMic] = createSignal(false);
  const [fpfc, setFpfc] = createSignal(false);
  const [gameVolume, setGameVolume] = createSignal(0);
  const [micVolume, setMicVolume] = createSignal(0);
  const [micThreshold, setMicThreshold] = createSignal(0);
  const [micMix, setMicMix] = createSignal(0);

  const [hasSettings, setHasSettings] = createSignal(false);

  ON_MESSAGE.addListener((message) => {
    if (message.$case !== "settings") return;
    const settings = message.value;

    console.log("got settings", settings);

    batch(() => {
      setWidth(settings.horizontal);
      setHeight(settings.vertical);
      setBitrate(settings.bitrate);
      setFps(settings.fps);
      setFov(settings.fov);
      setSmoothness(settings.smoothness);
      setMic(settings.mic);
      setFpfc(settings.fpfc);
      setGameVolume(settings.gameVolume);
      setMicVolume(settings.micVolume);
      setMicThreshold(settings.micThreshold);
      setMicMix(settings.micMix);

      setHasSettings(true);
    });
  });

  ON_DISCONNECT.addListener(() => setHasSettings(false));

  createEffect(() => {
    if (hasSettings())
      send({
        $case: "settings",
        value: {
          horizontal: width(),
          vertical: height(),
          bitrate: bitrate(),
          fps: fps(),
          fov: fov(),
          smoothness: smoothness(),
          mic: mic(),
          fpfc: fpfc(),
          gameVolume: gameVolume(),
          micVolume: micVolume(),
          micThreshold: micThreshold(),
          micMix: micMix(),
        },
      });
  });

  return {
    width,
    setWidth,
    height,
    setHeight,
    bitrate,
    setBitrate,
    fps,
    setFps,
    fov,
    setFov,
    smoothness,
    setSmoothness,
    mic,
    setMic,
    fpfc,
    setFpfc,
    gameVolume,
    setGameVolume,
    micVolume,
    setMicVolume,
    micThreshold,
    setMicThreshold,
    micMix,
    setMicMix,
    hasSettings,
  };
}

const OptionsContext = createContext<ReturnType<typeof makeOptionsContext>>();

export function OptionsProvider(props: ParentProps) {
  const val = makeOptionsContext();

  return (
    <OptionsContext.Provider value={val}>
      {props.children}
    </OptionsContext.Provider>
  );
}

export const useOptions = () => useContext(OptionsContext)!;

export function OptionsMenu() {
  const [resolution, setResolution] = createSignal("");

  const {
    width,
    setWidth,
    height,
    setHeight,
    bitrate,
    setBitrate,
    fps,
    setFps,
    fov,
    setFov,
    smoothness,
    setSmoothness,
    mic,
    setMic,
    fpfc,
    setFpfc,
    gameVolume,
    setGameVolume,
    micVolume,
    setMicVolume,
    micThreshold,
    setMicThreshold,
    micMix,
    setMicMix,
    hasSettings,
  } = useOptions();

  createEffect(() => {
    const currW = width();
    const currH = height();
    for (const [str, [w, h]] of Object.entries(resolutions)) {
      if (currW === w && currH === h) {
        setResolution(str);
        break;
      }
    }
  });

  createEffect(() => {
    const str = resolution();
    if (!(str in resolutions)) return;
    const [w, h] = resolutions[str as keyof typeof resolutions];
    batch(() => {
      setWidth(w);
      setHeight(h);
    });
  });

  const micMixString = () => micMixModes.at(micMix()) ?? "Invalid";
  const setMicMixString = (value: string) =>
    setMicMix(micMixModes.indexOf(value));

  return (
    <Popover placement="bottom-end">
      <PopoverTrigger as={Button} class="w-10 p-1">
        <OptionsIcon />
      </PopoverTrigger>
      <PopoverContent class="w-56 flex flex-col gap-4">
        <Show
          when={hasSettings()}
          fallback={
            <div class="w-full h-16 flex justify-center items-center">
              <Spinner size={8} />
            </div>
          }
        >
          <Select
            name="Resolution"
            value={resolution()}
            onChange={setResolution}
            options={Object.keys(resolutions)}
          />
          <Slider
            name="Bitrate"
            value={bitrate()}
            onChange={setBitrate}
            min={1000}
            max={20000}
            step={1000}
            label={(val) => `${val} kbps`}
          />
          <Slider
            name="FPS"
            value={fps()}
            onChange={setFps}
            min={10}
            max={90}
            step={5}
          />
          <Slider
            name="FOV"
            value={fov()}
            onChange={setFov}
            min={50}
            max={100}
          />
          <Separator />
          <Slider
            name="Smoothness"
            value={smoothness()}
            onChange={setSmoothness}
            min={0}
            max={2}
            step={0.1}
            label={(val) => val.toFixed(1)}
          />
          <Switch checked={mic()} onChange={setMic}>
            <SwitchControl>
              <SwitchThumb />
            </SwitchControl>
            <SwitchLabel>Mic Audio</SwitchLabel>
          </Switch>
          <Switch checked={fpfc()} onChange={setFpfc}>
            <SwitchControl>
              <SwitchThumb />
            </SwitchControl>
            <SwitchLabel>FPFC</SwitchLabel>
          </Switch>
          <Separator />
          <Slider
            name="Game Volume"
            value={gameVolume()}
            onChange={setGameVolume}
            min={0}
            max={2}
            step={0.1}
            label={(val) => val.toFixed(1)}
          />
          <Slider
            name="Mic Volume"
            value={micVolume()}
            onChange={setMicVolume}
            min={0}
            max={2}
            step={0.1}
            label={(val) => val.toFixed(1)}
          />
          <Slider
            name="Mic Threshold"
            value={micThreshold()}
            onChange={setMicThreshold}
            min={0}
            max={2}
            step={0.1}
            label={(val) => val.toFixed(1)}
          />
          <Select
            name="Mic Mixing"
            value={micMixString()}
            onChange={setMicMixString}
            options={micMixModes}
          />
        </Show>
      </PopoverContent>
    </Popover>
  );
}
