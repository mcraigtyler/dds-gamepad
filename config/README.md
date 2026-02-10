# Config File Format

`dds-gamepad` uses **role-based YAML** config files (for example `driver.yaml` and `gunner.yaml`).
Each file defines one role and a list of mappings.

## Top-level structure

```yaml
role:
  name: "Driver"

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
    gamepad:
      to: axis:right_x
      scale: 1.0
      deadzone: 0.02
      invert: false
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
  - `gamepad.to` (string)

## Optional fields

- `dds.input_min` + `dds.input_max` (float; must be provided together)
- `gamepad.scale` (float, default `1.0`)
- `gamepad.deadzone` (float, default `0.0`; must be in `[0.0, 1.0]` for axis/trigger mappings)
- `gamepad.invert` (bool, default `false`)

## Allowed DDS values

- `dds.type`:
  - `Gamepad::Gamepad_Analog` or `Gamepad_Analog`
  - `Gamepad::Stick_TwoAxis` or `Stick_TwoAxis`
  - `Gamepad::Button` or `Button`

- `dds.field` by type:
  - `Gamepad::Gamepad_Analog`: `value`
  - `Gamepad::Stick_TwoAxis`: `x` or `y`
  - `Gamepad::Button`: `btnState`

## Allowed gamepad targets (`gamepad.to`)

- Axis/trigger:
  - `axis:left_trigger`
  - `axis:right_trigger`
  - `axis:left_x`
  - `axis:left_y`
  - `axis:right_x`
  - `axis:right_y`
- Buttons:
  - `button:a`
  - `button:x`
- D-Pad:
  - `dpad:up`
  - `dpad:down`
  - `dpad:left`
  - `dpad:right`

## Runtime filtering (`yoke_id`)

`yoke_id` is **not configured in YAML**.
Provide it at runtime:

- Console app: `<config_file> <domain_id> <yoke_id>`
- Service: `--domain-id <n> --yoke-id <sub_role> --config-file <path>`

Only DDS messages whose resolved bus identifier has a matching `sub_role` are processed.
