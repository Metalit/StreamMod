import { type ClassValue, clsx } from "clsx";
import { createEffect, createSignal, Signal, SignalOptions } from "solid-js";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function createPersistentSignal<T>(
  key: string,
  defaultVal: T,
  fromString?: (value: string) => T,
  toString?: (value: T) => string,
  options?: SignalOptions<T>
): Signal<T> {
  const stored = localStorage.getItem(key);
  const [val, setVal] = createSignal(
    stored ? fromString?.(stored) ?? (stored as T) : defaultVal,
    options
  );
  createEffect(() => {
    localStorage.setItem(key, toString ? toString(val()) : String(val()));
  });
  return [val, setVal];
}

// me when I have to implement my own resizable array in js of all languages
export class Uint8ArrayList {
  arr = new Uint8Array(8);
  size = 0;

  add(data: Uint8Array) {
    if (this.size + data.length <= this.arr.length)
      this.arr.set(data, this.size);
    else {
      const bigger = new Uint8Array(
        Math.max(this.size + data.length, this.arr.length * 2)
      );
      bigger.set(this.arr);
      bigger.set(data, this.size);
      this.arr = bigger;
    }
    this.size += data.length;
  }

  get() {
    return this.arr.subarray(0, this.size);
  }

  clear() {
    this.size = 0;
  }
}
