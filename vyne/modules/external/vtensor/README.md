# vtensor - device tensor metadata for Vyne

`vtensor` is the small metadata layer over `@device` handles. It does not own
device memory and it does not allocate by itself. A `DeviceTensor` pairs an
`Int64` device handle with HWC shape and element count so higher-level device
helpers can pass shape safely without long argument lists.

```vyne
use external "vtensor/vtensor.vy";

@device<cuda> region r {
    h :: Int64 := device_alloc(64 * 64 * 3);
    x :: vtensor.DeviceTensor := vtensor.tensor(h, 64, 64, 3);
};
```

## Rules

- Handles are region-local. Do not store `DeviceTensor` values outside the
  `@device` region that owns their handles.
- V1 is HWC-only because it targets image models first.
- Helpers marked `@device_fn` are callable only from active `@device` regions or
  other `@device_fn` helpers.

## API

| Function | Result |
| :--- | :--- |
| `vtensor.tensor(handle, h, w, c)` | shape metadata for an existing handle |
| `vtensor.copy_slice(dst, packed, offset)` | device-side packed-buffer crop staging |
