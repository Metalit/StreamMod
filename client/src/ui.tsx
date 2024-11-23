import {
  Slider as BasicSlider,
  SliderFill,
  SliderLabel,
  SliderThumb,
  SliderTrack,
  SliderValueLabel,
} from "./components/ui/slider";
import {
  TextField,
  TextFieldInput,
  TextFieldLabel,
} from "./components/ui/text-field";
import {
  Select as BasicSelect,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "./components/ui/select";
// @ts-expect-error
import BasicSpinner from "solidjs-material-spinner";

export function Slider(props: {
  name: string;
  value?: number;
  onChange?: (val: number) => void;
  min: number;
  max: number;
  step?: number;
  label?: (val: number) => string;
  class?: string;
}) {
  const value = () => {
    const value = props.value;
    if (!value) return undefined;
    return [value];
  };

  return (
    <BasicSlider
      value={value()}
      onChange={(value) => props.onChange?.(value[0])}
      minValue={props.min}
      maxValue={props.max}
      step={props.step}
      getValueLabel={({ values }) =>
        props.label?.(values[0]) ?? values[0].toString()
      }
      class={props.class}
    >
      <div class="flex w-full mb-1 justify-between">
        <SliderLabel>{props.name}</SliderLabel>
        <SliderValueLabel />
      </div>
      <SliderTrack>
        <SliderFill />
        <SliderThumb />
      </SliderTrack>
    </BasicSlider>
  );
}

export function Input(props: {
  name: string;
  value: string;
  onChange: (val: string) => void;
  number?: boolean;
  class?: string;
}) {
  return (
    <TextField class={props.class}>
      <TextFieldLabel>{props.name}</TextFieldLabel>
      <TextFieldInput
        type={props.number ? "number" : "text"}
        value={props.value}
        onInput={(event) => props.onChange(event.currentTarget.value)}
      />
    </TextField>
  );
}

export function Select(props: {
  name: string;
  value: string;
  onChange: (val: string) => void;
  options: string[];
}) {
  return (
    <BasicSelect
      value={props.value}
      onChange={(val) => props.onChange(val ?? "")}
      options={props.options}
      itemComponent={(props) => (
        <SelectItem item={props.item}>{props.item.rawValue}</SelectItem>
      )}
    >
      <TextFieldLabel>{props.name}</TextFieldLabel>
      <SelectTrigger>
        <SelectValue<string>>{(state) => state.selectedOption()}</SelectValue>
      </SelectTrigger>
      <SelectContent />
    </BasicSelect>
  );
}

export function Spinner(props: { size: number }) {
  return (
    <BasicSpinner radius={props.size * 4} stroke={3} color="var(--spinner-color)" />
  );
}
