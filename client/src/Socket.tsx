import {
  batch,
  createContext,
  createSignal,
  onCleanup,
  ParentProps,
  useContext,
} from "solid-js";
import { DeepPartial, Exact, PacketWrapper } from "~/proto/stream";

export class EventListener<T> {
  private listeners: (((val: T) => void) | undefined)[] = [];

  addListener(callback: (val: T) => void, cleanup = true, once = false) {
    if (once) {
      const original = callback;
      callback = (v: T) => {
        original(v);
        this.removeListener(callback);
      };
    } else if (cleanup) onCleanup(this.removeListener.bind(this, callback));
    this.listeners.push(callback);
    return callback;
  }

  removeListener(callback: (val: T) => void) {
    const index = this.listeners.findIndex((l) => l === callback);
    if (index >= 0) this.listeners[index] = undefined;
  }

  invoke(value: T) {
    [...this.listeners].forEach((callback) => {
      callback?.(value);
    });
  }
}

function makeSocketContext() {
  let socket: WebSocket | undefined;
  let reconnect = false;

  const [currentIp, setCurrentIp] = createSignal<string>();
  const [currentPort, setCurrentPort] = createSignal<string>();

  const ON_CONNECT = new EventListener<void>();
  const ON_DISCONNECT = new EventListener<void>();
  const ON_ERROR = new EventListener<Event>();
  const ON_MESSAGE = new EventListener<NonNullable<PacketWrapper["Packet"]>>();

  const connect = (ip: string, port: string) => {
    if (
      socket?.readyState === WebSocket.OPEN &&
      ip === currentIp() &&
      port === currentPort()
    )
      return;

    disconnect();

    batch(() => {
      setCurrentIp(ip);
      setCurrentPort(port);
      reconnect = true;
      socket = new WebSocket(`ws://${ip}:${port}`);
      socket.binaryType = "arraybuffer";
      socket.onopen = onOpen;
      socket.onerror = onError;
      socket.onclose = onClose;
      socket.onmessage = onMessage;
    });
  };

  const disconnect = () => {
    reconnect = false;
    socket?.close();
  };

  const connection = () => {
    const ip = currentIp();
    const port = currentPort();
    if (!ip || !port) return undefined;
    return { ip, port };
  };

  const send = <I extends Exact<DeepPartial<PacketWrapper["Packet"]>, I>>(
    message: I
  ) => {
    socket?.send(
      PacketWrapper.encode(PacketWrapper.create({ Packet: message })).finish()
    );
  };

  const onOpen = () => {
    console.log("socket connected");
    ON_CONNECT.invoke();
  };

  const onError = (e: Event) => {
    console.log("socket error", e);
    ON_ERROR.invoke(e);
  };

  const onClose = (e: CloseEvent) => {
    console.log("socket disconnected", reconnect, e);
    socket?.close();
    if (reconnect) {
      setTimeout(() => {
        if (reconnect) connect(currentIp()!, currentPort()!);
        else onClose(e);
      }, 2000);
    } else {
      ON_DISCONNECT.invoke();
      batch(() => {
        setCurrentIp(undefined);
        setCurrentPort(undefined);
      });
    }
  };

  const onMessage = (e: MessageEvent<ArrayBuffer>) => {
    const { Packet: packet } = PacketWrapper.decode(new Uint8Array(e.data));
    if (packet === undefined) console.error("received invalid packet");
    else ON_MESSAGE.invoke(packet);
  };

  return {
    connect,
    disconnect,
    connection,
    send,
    ON_CONNECT,
    ON_DISCONNECT,
    ON_ERROR,
    ON_MESSAGE,
  };
}

const SocketContext = createContext<ReturnType<typeof makeSocketContext>>();

export function SocketProvider(props: ParentProps) {
  const val = makeSocketContext();

  return (
    <SocketContext.Provider value={val}>
      {props.children}
    </SocketContext.Provider>
  );
}

export const useSocket = () => useContext(SocketContext)!;
