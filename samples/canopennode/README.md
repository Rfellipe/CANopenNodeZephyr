# CANopenNode CiA 402 Zephyr sample

## Overview

This sample demonstrates CANopenNode and the CiA 402 drive profile on Zephyr. It supplies a generated
object dictionary and a simulated motor-control backend implementing profile position, velocity,
torque, homing, CSP, CSV, and CST callbacks.

The simulator applies targets immediately. It demonstrates integration and protocol behavior; it
is not a physical motor-control or safety implementation.

See the repository's [Zephyr CiA 402 integration guide](../../docs/cia402-zephyr.md) for module
installation, object-dictionary requirements, initialization order, callback semantics, and
threading requirements.

## Requirements

- A Zephyr board with a classic CAN controller.

- A CAN transceiver when the selected board does not include one.

- A second CANopen node or PC CAN adapter for testing on physical hardware.

The board devicetree must provide the `zephyr,canbus` chosen node. The supplied overlays configure
the supported sample boards.

## Configuration

The default [prj.conf](prj.conf) enables CANopenNode, CiA 402, the module-managed SYNC thread, and
CANopen status LEDs. The CiA 402 configuration currently disables the legacy object-dictionary
storage and program-download adapters.

The sample node ID defaults to 10 and is configurable through `CONFIG_CANOPEN_NODE_ID`.

## Build and run

When this repository is already a project in the active West manifest:

```shell
west build -b <board> canopennodezephyr/samples/canopennode
west flash
```

When building directly from a checkout that is not registered in the active manifest:

```shell
west build -b <board> samples/canopennode -- \
  -DZEPHYR_EXTRA_MODULES="$PWD"
west flash
```

For the native simulator:

```shell
west build -b native_sim/native/64 samples/canopennode -- \
  -DZEPHYR_EXTRA_MODULES="$PWD"
west build -d build -t run
```

A successful start prints:

```text
CANopen stack initialized
```

If the controller has fewer receive filters than the object dictionary requires, the driver prints
a warning and uses software CAN-ID dispatch. This is expected on `native_sim`, which exposes 16
filters while the sample requests 17.

## Supported boards

The repository contains configuration or overlays for:

- `native_sim/native/64`;

- `twr_ke18f`;

- `frdm_k64f`;

- `stm32f072b_disco`;

- `stm32f3_disco`; and

- `stm32h573i_dk`.

Boards without an onboard CAN transceiver require an external transceiver and board-appropriate CAN
pin connections. Consult the Zephyr board documentation and schematic before enabling the bus.

## Drive behavior

After boot, place node 10 into the NMT operational state. A CANopen manager can then write the CiA
402 controlword at `0x6040` and select a mode through `0x6060`. Read `0x6041` for the state-machine
status and `0x6061` for the accepted mode.

A typical transition to Operation Enabled writes these controlwords in order:

1. `0x0006` — Shutdown, moving to Ready to Switch On.
2. `0x0007` — Switch On, moving to Switched On.
3. `0x000F` — Enable Operation, moving to Operation Enabled.

The common sample targets are:

| Mode | Value | Target object |
| --- | ---: | --- |
| Profile position | 1 | `0x607A` |
| Velocity | 2 | `0x6042` |
| Profile velocity | 3 | `0x60FF` |
| Profile torque | 4 | `0x6071` |
| Homing | 6 | `0x6098` |
| CSP | 8 | `0x607A` |
| CSV | 9 | `0x60FF` |
| CST | 10 | `0x6071` |

Profile position and homing use the new-set-point bit in the controlword. CSP, CSV, and CST commands
are applied only on a received SYNC while the node is NMT operational and the CiA 402 state is
Operation Enabled.

## Replace the simulator with hardware

The simulated backend is in [src/main.c](src/main.c). Replace its callbacks with an application
object that controls the motor driver and reports measured position, velocity, torque, limits, and
faults.

State transitions and profile callbacks execute from the main CANopen thread. Cyclic callbacks
execute from the SYNC thread after synchronous RPDO processing and before synchronous TPDO
processing. Cyclic callbacks must not sleep, allocate memory, or perform unbounded work.

Hardware protections such as safe torque off, overcurrent shutdown, watchdogs, travel limits, and
emergency stop must remain independent of CANopen communication.

## Test

Run the Zephyr sample test with:

```shell
west twister -T samples/canopennode -p native_sim/native/64 --inline-logs
```

The lower-level state-machine tests reside in the CANopenNode submodule:

```shell
make -C CANopenNode/test test
```

## Modify the object dictionary

Edit [objdict/objdict.eds](objdict/objdict.eds) or the corresponding XML project and regenerate the
CANopenNode v4 `CO_OD.c`, `CO_OD.h`, and `OD.h` files. Preserve the mandatory CiA 402 entries and
`OD_CNT_CIA402=1`. Review PDO mapping, access attributes, default limits, and units before using the
generated dictionary on real hardware.
