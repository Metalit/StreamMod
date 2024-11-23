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
