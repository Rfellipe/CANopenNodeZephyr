# CANopenNode Protocol Stack for Zephyr RTOS

## Introduction

[CANopenNode](https://github.com/CANopenNode/CANopenNode) is a free and open source CANopen protocol
stack. This repository provides glue code for integrating the stack with the [Zephyr
Project](https://github.com/zephyrproject-rtos/zephyr). A pinned CANopenNode fork containing the
CiA 402 implementation is included as a Git submodule.

Both CANopenNode and CANopenNodeZephyr are licensed under the Apache-2.0 license.

For the complete setup, object-dictionary, callback, threading, and build instructions, see
[Using the CiA 402 library with Zephyr](docs/cia402-zephyr.md).

## CANopenNode as a Zephyr Module

To pull in CANopenNodeZephyr as a Zephyr module, either add it as a West project in the `west.yaml`
file or pull it in by adding a submanifest (e.g. `zephyr/submanifests/canopennodezephyr.yaml`) file
with the following content and run `west update`:

```yaml
manifest:
  projects:
    - name: canopennodezephyr
      url: https://github.com/rfellipe/CANopenNodeZephyr.git
      revision: main
      submodules:
        - path: CANopenNode
      path: custom/canopennodezephyr # adjust the path as needed
```

Enable the drive profile in the application configuration:

```ini
CONFIG_CAN=y
CONFIG_CANOPENNODE=y
CONFIG_CANOPENNODE_CIA402=y
CONFIG_CANOPENNODE_SYNC_THREAD=y
```

The application object dictionary must define `0x6040`, `0x6041`, `0x6060`, and `0x6061`.
Register the motor-control backend with `CO_CANopenInitCiA402Hw()` after `CO_CANopenInit()` and
before entering CAN normal mode. Cyclic synchronous modes are executed after synchronous RPDOs and
before synchronous TPDOs by the module's SYNC thread.

Object-dictionary storage and CANopen program download are currently unavailable when CiA 402 is
enabled. Their existing adapters still use the legacy generated-object-dictionary layout.

## Integration changes

The current integration includes:

- a software receive-filter fallback for CAN controllers with fewer hardware filters than the
  CANopen object dictionary requires;
- synchronous CSP, CSV, and CST execution between synchronous RPDO and TPDO processing;
- synchronization between the sample's main CANopen loop, SYNC thread, and communication-reset
  path;
- CiA 402-enabled and CiA 402-disabled object-dictionary builds; and
- native Zephyr and standalone CiA 402 tests.
