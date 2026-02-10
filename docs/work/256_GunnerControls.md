# Gunner Mappings
This task is to add mappings for a Gunner by listening for DDS messages and converting those messages to Gamepad inputs.

We already have mapping configs for Steering and Throttle, now we need mappings for new inputs for gunner functions: Fire, Swap Projectile, Coax Select, Main Gun Select, Zoom 12x, Zoom 3x and Turret movement. Since we have so many different configs to map I think making one config file per Role makes the most sense. So we would have a Driver config and a Gunner config and each would list all the mappings for that role. When we run the app we would specify which specific role config we want to load.

### Butons
The table mappings below describe inputs that will come in as Button inputs from DDS. See [Button Log](../../test-data/gamepad_button.log) for sample DDS data. In this data you will see events with btnChanging=True where the btnState field is changing values. ButtonState of Down should translate the a button press on the gamepad and stay down until a ButtonState Up is received.

These are the events we care about to indicate we need to change button state from not being pressed to being pressed, and vice versa.
- `Button(id=BusIdentifier_t(role=1, sub_role=1004), another_id=BusIdentifier_t(role=0, sub_role=0), btnState=<ButtonState_t.Up: 2>, btnChanging=True, yet_another_id=BusIdentifier_t(role=0, sub_role=0), just_a_series=[])`
- `Button(id=BusIdentifier_t(role=1, sub_role=1004), another_id=BusIdentifier_t(role=0, sub_role=0), btnState=<ButtonState_t.Up: 2>, btnChanging=False, yet_another_id=BusIdentifier_t(role=0, sub_role=0), just_a_series=[])`

- `Button(id=BusIdentifier_t(role=1, sub_role=1004), another_id=BusIdentifier_t(role=0, sub_role=0), btnState=<ButtonState_t.Down: 2>, btnChanging=True, yet_another_id=BusIdentifier_t(role=0, sub_role=0), just_a_series=[])`
- `Button(id=BusIdentifier_t(role=1, sub_role=1004), another_id=BusIdentifier_t(role=0, sub_role=0), btnState=<ButtonState_t.Down: 2>, btnChanging=False, yet_another_id=BusIdentifier_t(role=0, sub_role=0), just_a_series=[])`
- `Button(id=BusIdentifier_t(role=1, sub_role=1004), another_id=BusIdentifier_t(role=0, sub_role=0), btnState=<ButtonState_t.Down: 2>, btnChanging=False, yet_another_id=BusIdentifier_t(role=0, sub_role=0), just_a_series=[])`
- `Button(id=BusIdentifier_t(role=1, sub_role=1004), another_id=BusIdentifier_t(role=0, sub_role=0), btnState=<ButtonState_t.Down: 2>, btnChanging=False, yet_another_id=BusIdentifier_t(role=0, sub_role=0), just_a_series=[])`

- `Button(id=BusIdentifier_t(role=1, sub_role=1004), another_id=BusIdentifier_t(role=0, sub_role=0), btnState=<ButtonState_t.Up: 2>, btnChanging=True, yet_another_id=BusIdentifier_t(role=0, sub_role=0), just_a_series=[])`

| Action | Controller Input | DDS Input ID |
|----|------------------|---------------|
| Fire |  A    | 3
| Fire  |  A    | 6
| Swap Projectile | X | 7 button |
| Coax/Main | DPad Left/Right | 4 button (Coax) <br/> 5 button (Main) |
| Zoom 12x/3x | DPad Up/Down | 2 button (12x) <br/> 1 button (3x) |

### Analog
| Action | Controller Input | DDS Input ID |
|--------|------------------|----------------|
| Turret | Left Stick | 30[-110,110]  (x-axis -left/+right) <br/> 1[0,1] analog (Down) <br/> 2[0,1] analog (Up) |

For the Turret DDS messages will come in from 3 different sources to control its movement. The left/right movement will be controlled from the Analog input ID 30 on the x-axis with values ranging from -110 to +110 with negative values turning left and postive values turning right. The Analog input ID 1 will move the turret Down and the Analog input ID 2 will move the turret Up. Both Analog input 1 and 2 have values ranging from 0 to 1.

### Config File Schema Changes
 Config files will need to be Role based instead of Input Mapping based. This means instead of having a config file per input mapping we would have a config file that has multiple mappings. Each mapping section would contain the topic, type and idl file it uses. The header information of the config file would contain the yoke sub_role id which will be used to filter messages that contain that sub_role id.

 There is no need to be backward compatible with the old config format, just move forward with the new format only.

**Proposed Role based Yaml**
```yaml
role:
  name: "Driver"
  yoke_id: 1004

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

  - name: throttle
    dds:
      topic: "Gamepad_Analog"
      type: "Gamepad::Gamepad_Analog"
      idl_file: "idl/Gamepad.idl"
      id: 2
      field: value
    gamepad:
      to: axis:right_trigger
      scale: 1.0
      deadzone: 0.05
      invert: false

  - name: brake
    dds:
      topic: "Gamepad_Analog"
      type: "Gamepad::Gamepad_Analog"
      idl_file: "idl/Gamepad.idl"
      id: 1
      field: value
    gamepad:
      to: axis:left_trigger
      scale: 1.0
      deadzone: 0.05
      invert: false
```

## Tasks
- [x] 256.1 Change config files to be by Role instead of by individual input mapping.
- [ ] 256.2 Modify command line to accept a single config file instead of a folder. Log the file being loaded and the mappings being loaded. Update the README's ([Scripts/README](../../Scripts/README.md), [README](../../README.md)).
- [ ] 256.3 Modify Windows Service to accept a ConfigFilePath at Install time. This will serve the same purpose as the command line config path in 256.2 and should call the same undlerying methods to make use of the specified config file.
- [ ] 256.4 Modify code to read new config file format in the new role based configs: [driver.yaml](../../config/driver.yaml) and [gunner.yaml](../../config/gunner.yaml).
- [ ] 256.7 Update the install target to include the new role based configs.
- [ ] 256.5 Read Gamepad Button messages to support [gunner.yaml](../../config/gunner.yaml).
- [ ] 256.6 Make use of the `yoke_id` to filter out DDS messages so that we only recieve messages where the `yoke_id` from the config matches the `sub_role` of the incoming DDS message.
