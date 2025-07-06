# Data Link Layer

This component represents the data link layer. It will handle board to board communication (abstracting away the RMT specific details of transmitting physical raw bits over wires).

## Frame Definitions

See `./include/Frames.h` for frame definitions.

There will be two types of frames: Control and Generic.

Control frames will contain all control information (eg. Spinning a DC motor, moving a servo to a particular angle, sensor information). These frames will have a limit of 32B of data size with a total packet size of 41B. These frames will not be fragmented.

Generic frames will contain all other information. They will have a max data size of 8KiB with a total packet size of 8206B (8.013672 KiB). These frames can be fragmented.

To differentiate between the two frames, the `type` field in both frames will be compared against. If the MSB is set to 1, it will be determined to be a control packet.

## Routing Information Protocol (RIP) Table

See `./include/Tables.h` for table definitions.

WIP