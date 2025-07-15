# Adding New Modules

## Adding Actuators
1. Append the actuator you want to add to the enum in the [RobotModule flatbuffer definition](https://git.uwaterloo.ca/capstone-group2/flatbuffers/-/blob/main/RobotModule.fbs?ref_type=heads)
- Propagate this change to this repository in the [flatbuffer](https://git.uwaterloo.ca/capstone-group2/firmware/-/tree/main/components/flatbuffers?ref_type=heads) component, and the [Control library](https://git.uwaterloo.ca/capstone-group2/control).
2. Update any of the entries in the [Constants module](https://git.uwaterloo.ca/capstone-group2/firmware/-/blob/main/components/constants/include/constants/module.h?ref_type=heads) that depend on the module type (for instance, `MODULE_TO_NUM_CHANNELS_MAP`).
3. Create a header file for the module, implementing `IActuator`.
4. Implement the actuator
- Deserialize the control message with Flatbuffers to match the message sent by the control library.
- Do not block for too long in the `actuate` function, as this will block the main control loop.
4. Add the actuator to the `ActuatorFactory`.
