WIP

# Preprogrammed Movement

This component will handle the preprogrammed movements from the UI and then execute them (with or without a) connection to the PC/UI. 

See [ESP32 NVS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html) for more information about the NVS.

## Movement Entries Stored on NVS

The movements will be stored on the NVS of the leader board on the network, under the namespace `MOVEMENTS_NVS_NAMESPACE`.

The movements themselves will be a table under the key `MOVEMENTS_NVS_KEY` as a blob:

```
{
    num_movements: uint8_t,
    movements: MovementEntry[]
}
```

where `MovementEntry` is:

```
{
	board_id: uint16_t,
	moduleType: uint8_t,
	value_action: uint16_t,
	condition: ConditionBlob,
	ack: uint8_t,
	ack_ttl_ms: uint16_t,
	post_delay_ms: uint16_t
}
```

and `ConditionBlob` is:

```
{
	value: uint16_t,
	cond: uint8_t (0: <, 1: <=, 2: >, 3: >=, 4: ==, 5: !=),
	moduleType: uint8_t (check value/sensor data if possible or set to UNUSED if not in use)
}
```

Each entry will have a size of 14B. With a max number of 255 movements stored, that will represent 3570 B that will be stored on the NVS.

There will only be one set of movements stored on the board (meaning a new movement set would have to overwrite the existing movement set).

Blobs are written in the NVS with the entry metadata written first, then the actual data written in subsequent entries (32B per entry).

## Storing the Toplogy for the Movement Set

To run the movement set stored on a particular board, the board itself has to be the leader on the network (to resolve conflicts with multiple movement sets stored on different boards).

Since the boards on the network will most likely have different ids assigned, the leader will have manually assign ids based on the expected toplogy that will correspond to the saved movement set. This is done to ensure the correct board is being properly referenced in the movement set. However, the stored board id on the NVS will not be overwritten with the board id assigned by the movement set but rather a simple 1:1 translation map will be used (making the movement-assigned board id functionally in use but the underlying board id will be used purely for wired communications).

In order to do this, the toplogy will need to be saved onto the NVS (along with the movement entries). This will be stored in the same namespace but under a different key, `MOVEMENTS_NVS_TOPOLOGY_KEY`.

The structure of the topology data stored on the NVS will look like:

```
{
    boards: NeighbourBlob[] (index 0 is leader)
}
```

where `NeighbourBlob` is:

```
{
    curr_board_id: uint16_t,
	neighbour_connections: ChannelBoardConn[] (max 4, min 1)
}
```

where `ChannelBoardConn` is:
```
{
    channel: uint8_t,
	board_id: uint16_t
}
```

