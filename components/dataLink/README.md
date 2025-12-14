WIP

# Data Link Layer

This component represents the data link layer. It will handle board to board communication (abstracting away the RMT specific details of transmitting physical raw bits over wires).

## Frame Definitions

See `./include/Frames.h` for frame definitions.

There will be two types of frames: Control and Generic.

Control frames will contain all control information (eg. Spinning a DC motor, moving a servo to a particular angle, sensor information). These frames will have a limit of 256B of data size with a total packet size of 41B. These frames will not be fragmented.

Generic frames will contain all other information. They will have a max data size of 16MiB if fragmented (max 2**16 fragments). Each fragment will still maintain a max 256B data size.

To differentiate between the two frames, the `type` field in both frames will be compared against. If the MSB is set to 1, it will be determined to be a control packet.

## Routing Information Protocol (RIP) Table

See `./include/Tables.h` for table definitions. See `DataLinkRIP.cpp` for function definitions. 

It handles all known (advertised) boards on the same network of ESP32-S3 boards. It will broadcast its stored routing table to its neighbours roughly every 30 seconds or upon receiving a routing table from its neighbours. All routes on a routing table will expire after 180 seconds, unless refreshed/updated/re-advertised by that respective board. The routing table itself will be used to route frames using the path with the least number of hops (known locally).

## Frame Management

See `DataLinkFrames.cpp` for more information. 

It handles generic frame fragmentation, user handling of receiving received frames (stored in a queue, per channel), and a thread to handle polling the available channels for receiving. 

### Send Frames Scheduler

See `DataLinkScheduler.cpp` for more information.

It handles all TX frames passed from the user and schedules them to be sent. The frames are stored in a priority queue (per channel), where Control frames has a higher priority than Generic frames (generally). The actual scheduler algorithm can be found in `Scheduler.h`.

