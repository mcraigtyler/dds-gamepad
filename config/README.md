# Config File Format

`dds-gamepad` uses **role-based YAML** config files (for example `driver.yaml` and `gunner.yaml`).
Each file defines one role, an optional output backend, and a list of mappings.

## Top-level structure

```yaml
role:
  name: "Driver"

# Optional — selects the output backend. Defaults to vigem_x360 if absent.
output:
  type: vigem_x360        # "vigem_x360" or "udp_protobuf"
  # host: 192.168.1.100   # udp_protobuf only
  # port: 5000            # udp_protobuf only

mappings:
  - name: steering
    dds:
      topic: "Gamepad_Stick_TwoAxis"
      type: "Gamepad::Stick_TwoAxis"
      idl_file: "idl/Gamepad.idl"
      id: 30
      field: x
      input_min: -110.0
      input_max: 110.0
    output:               # also accepted: gamepad: (legacy alias, backward compatible)
      to: axis:right_x
      scale: 1.0
      deadzone: 0.02
      invert: false
      # type: axis        # optional explicit channel type — see below
```

## Required fields

- `role.name` (string)
- `mappings` (list with at least one entry)
- For each mapping:
  - `name` (string)
  - `dds.topic` (string)
  - `dds.type` (string)
  - `dds.idl_file` (string)
  - `dds.id` (integer)
  - `dds.field` (string)
  - `output.to` (string) — also accepted as `gamepad.to`

## Optional fields

### Top-level `output:` section

| Field | Type | Default | Description |
|---|---|---|---|
| `output.type` | string | `vigem_x360` | Backend type: `vigem_x360` or `udp_protobuf` |
| `output.host` | string | — | UDP target host (`udp_protobuf` only) |
| `output.port` | integer | — | UDP target port (`udp_protobuf` only) |

### Per-mapping `output:` / `gamepad:` fields

| Field | Type | Default | Description |
|---|---|---|---|
| `output.scale` | float | `1.0` | Multiplier applied to the raw DDS value before deadzone/invert |
| `output.deadzone` | float | `0.0` | Dead band in `[0.0, 1.0]`; values within this band are set to 0 |
| `output.invert` | bool | `false` | Flip stick direction or map trigger as `1.0 - value` |
| `output.additive` | bool | `false` | Sum contributions from multiple sources targeting the same channel |
| `output.type` | string | inferred | Explicit channel type: `axis`, `trigger`, or `button` |
| `dds.input_min` + `dds.input_max` | float | — | Normalize raw values in `[input_min, input_max]` to `[-1, 1]` or `[0, 1]` |

## Allowed DDS values

- `dds.type`:
  - `Gamepad::Gamepad_Analog` or `Gamepad_Analog`
  - `Gamepad::Stick_TwoAxis` or `Stick_TwoAxis`
  - `Gamepad::Button` or `Button`

- `dds.field` by type:
  - `Gamepad::Gamepad_Analog`: `value`
  - `Gamepad::Stick_TwoAxis`: `x` or `y`
  - `Gamepad::Button`: `btnState`

## Output targets

### `vigem_x360` targets (`output.to`)

Channel type is inferred from the prefix; explicit `output.type` is optional.

| Target | Inferred type | Notes |
|---|---|---|
| `axis:left_trigger` | `trigger` | Normalised `[0, 1]` |
| `axis:right_trigger` | `trigger` | Normalised `[0, 1]` |
| `axis:left_x` | `axis` | Normalised `[-1, 1]` |
| `axis:left_y` | `axis` | Normalised `[-1, 1]` |
| `axis:right_x` | `axis` | Normalised `[-1, 1]` |
| `axis:right_y` | `axis` | Normalised `[-1, 1]` |
| `button:a` | `button` | `0.0` or `1.0` |
| `button:b` | `button` | `0.0` or `1.0` |
| `button:x` | `button` | `0.0` or `1.0` |
| `button:y` | `button` | `0.0` or `1.0` |
| `dpad:up` | `button` | `0.0` or `1.0` |
| `dpad:down` | `button` | `0.0` or `1.0` |
| `dpad:left` | `button` | `0.0` or `1.0` |
| `dpad:right` | `button` | `0.0` or `1.0` |

### `udp_protobuf` targets (`output.to`)

`output.to` is the protobuf field name, used verbatim. `output.type` is required since
the channel type cannot be inferred from a custom field name.

```yaml
output:
  type: udp_protobuf
  host: 192.168.1.100
  port: 5000

mappings:
  - name: steering
    dds: { ... }
    output:
      to: steering    # protobuf field name
      type: axis      # required: axis | trigger | button
      scale: 1.0
      deadzone: 0.02

  - name: throttle
    dds: { ... }
    output:
      to: throttle
      type: trigger
```

## Runtime filtering (`yoke_id`)

`yoke_id` is **not configured in YAML**.
Provide it at runtime:

- Console app: `<config_file> <domain_id> <yoke_id>`
- Service: `--domain-id <n> --yoke-id <sub_role> --config-file <path>`

Only DDS messages whose resolved bus identifier has a matching `sub_role` are processed.
