# Using the CiA 402 library with Zephyr

This module combines CANopenNode, a Zephyr CAN driver adapter, and a generic CiA 402 Power Drive
System state machine. The application supplies its object dictionary and a callback table that
connects the generic drive profile to the actual motor-control hardware.

## Add the module to a West workspace

Add the project to the application manifest. The CANopenNode submodule is required:

```yaml
manifest:
  projects:
    - name: canopennodezephyr
      url: https://github.com/rfellipe/CANopenNodeZephyr.git
      revision: main
      path: modules/lib/canopennodezephyr
      submodules:
        - path: CANopenNode
```

Run:

```shell
west update canopennodezephyr
```

When developing this repository outside the active West manifest, pass it explicitly to CMake:

```shell
west build -b <board> samples/canopennode -- \
  -DZEPHYR_EXTRA_MODULES="$PWD"
```

## Configure Zephyr

The minimum application configuration for a CiA 402 node is:

```ini
CONFIG_CAN=y
CONFIG_CANOPENNODE=y
CONFIG_CANOPENNODE_CIA402=y
```

The module's SYNC thread is recommended and is enabled by default:

```ini
CONFIG_CANOPENNODE_SYNC_THREAD=y
CONFIG_CANOPENNODE_SYNC_THREAD_PERIOD_US=1000
```

The period is a scheduling fallback and elapsed-time source. Synchronous motion callbacks execute
only when `CO_process_SYNC()` reports a SYNC event.

The application devicetree must select a CAN controller:

```dts
/ {
    chosen {
        zephyr,canbus = &can0;
    };
};

&can0 {
    status = "okay";
    bitrate = <500000>;
};
```

The exact controller node, pin control, transceiver, and bitrate properties are board-specific.
The module currently supports classic CAN and intentionally depends on `!CAN_FD_MODE`.

## Provide an object dictionary

Generate the CANopenNode v4 object dictionary and compile its generated C source into the
application. The sample uses:

```cmake
target_sources(app PRIVATE
  src/main.c
  objdict/CO_OD.c
)
zephyr_include_directories(objdict)
```

The following entries are mandatory for initializing the CiA 402 layer:

| Index | Type | Purpose |
| --- | --- | --- |
| `0x6040` | `UNSIGNED16` | Controlword |
| `0x6041` | `UNSIGNED16` | Statusword |
| `0x6060` | `INTEGER8` | Modes of operation |
| `0x6061` | `INTEGER8` | Modes of operation display |

Mode-specific entries are optional, but the corresponding mode cannot do useful work without its
targets and feedback values. Common entries include:

| Index | Purpose |
| --- | --- |
| `0x603F` | Error code |
| `0x6064` | Position actual value |
| `0x606C` | Velocity actual value |
| `0x6077` | Torque actual value |
| `0x607A` | Target position for profile position and CSP |
| `0x60FF` | Target velocity for profile velocity and CSV |
| `0x6071` | Target torque for profile torque and CST |
| `0x6098` | Homing method |
| `0x607D` | Software position limits |
| `0x6072` | Maximum torque |
| `0x6081`–`0x6086` | Profile and deceleration parameters |

Set `OD_CNT_CIA402` to one in the generated OD when the profile is present. The supplied sample OD
is a complete reference. PDO-map `0x6040`, `0x6060`, and the required target values for incoming
commands; map `0x6041`, `0x6061`, and the required actual values for outgoing feedback.

## Implement the hardware interface

Create one application-owned state object and one callback table:

```c
struct drive_backend {
    /* Motor-control state, driver handles, and latest feedback. */
};

static bool_t drive_set_voltage(void *object, bool_t enable)
{
    struct drive_backend *drive = object;

    /* Safely enable or disable the power stage. */
    return true;
}

static bool_t drive_get_feedback(void *object,
                                 CO_CiA402_feedback_t *feedback)
{
    struct drive_backend *drive = object;

    feedback->positionActualValue = /* measured position */ 0;
    feedback->velocityActualValue = /* measured velocity */ 0;
    feedback->torqueActualValue = /* measured torque */ 0;
    feedback->faultActive = false;
    feedback->faultCode = CO_CIA402_ERR_NONE;
    return true;
}

static const CO_CiA402_hwInterface_t drive_interface = {
    .setEnableVoltage = drive_set_voltage,
    .getFeedback = drive_get_feedback,
    /* Add the state, profile, homing, and cyclic-mode callbacks used by the drive. */
};

static struct drive_backend drive;
```

A missing mode callback means that mode is not implemented. Returning `false` reports a generic
drive fault. `getFeedback()` should always initialize the entire feedback structure and return
`false` when trustworthy feedback cannot be obtained.

The `CO_CiA402_motionConfig_t` passed to motion callbacks contains the current OD values for quick
stop, shutdown, disable-operation and halt behavior, position and torque limits, and profile
acceleration/deceleration parameters. The application remains responsible for enforcing any
additional electrical, mechanical, or safety limits.

## Initialize and process the stack

The required communication-reset order is:

1. Put CAN into configuration mode and disable the previous CANopen CAN module.
2. Call `CO_CANinit()`.
3. Call `CO_CANopenInit()` with the generated `OD` object.
4. Call `CO_CANopenInitPDO()`.
5. Register the backend with `CO_CANopenInitCiA402Hw()`.
6. Enter CAN normal mode with `CO_CANsetNormalMode()`.

The essential registration call is:

```c
CO_CANopenInitCiA402Hw(CO, &drive, &drive_interface);
```

Call `CO_process()` regularly from the application thread. It handles the CiA 402 state machine,
statusword, feedback, profile position, profile velocity, profile torque, and homing.

When `CONFIG_CANOPENNODE_SYNC_THREAD=y`, the module handles `CO_process_SYNC()`, synchronous RPDOs,
cyclic CiA 402 processing, and synchronous TPDOs. Do not process those again in the application.

When the module SYNC thread is disabled, the application must preserve this order:

```c
bool_t sync = CO_process_SYNC(CO, elapsed_us, NULL);

CO_process_RPDO(CO, sync, elapsed_us, NULL);
if (sync && CO->NMT != NULL) {
    CO_CiA402_processSync(CO->CiA402,
                          CO->NMT->operatingState == CO_NMT_OPERATIONAL,
                          elapsed_us);
}
CO_process_TPDO(CO, sync, elapsed_us, NULL);
```

## Callback execution and concurrency

State-machine, profile-mode, homing, and feedback callbacks normally execute in the application
thread that calls `CO_process()`.

CSP, CSV, and CST callbacks execute in the module SYNC thread when it is enabled. They run after
synchronous RPDO data becomes visible and before synchronous TPDO feedback is produced. Cyclic
callbacks must be deterministic, non-blocking, and safe to call from that thread.

The sample protects initialization, communication reset, deletion, `CO_process()`, and synchronous
processing with `CO_LOCK_OD()`/`CO_UNLOCK_OD()`. Applications using the module SYNC thread should
apply the same serialization when touching the stack lifecycle or OD from another thread.

`CO_CANmodule_disable()` clears `CANnormal` before stopping CAN. This prevents the SYNC thread from
starting another processing cycle during communication reset. The lock is still required to wait
for a cycle already in progress.

## CAN receive filters

CANopenNode may request more receive filters than a controller implements. The Zephyr adapter uses
individual hardware filters when sufficient filters exist. Otherwise it installs one broad
hardware filter and dispatches CANopen identifiers in software. A warning reports when this
fallback is active.

Software filtering prevents initialization failure, but increases receive-side CPU load on a busy
CAN network. Validate the worst-case bus load and callback latency on the target hardware.

## Build and test

Build and run the native sample from a configured West workspace:

```shell
west build -b native_sim/native/64 samples/canopennode -- \
  -DZEPHYR_EXTRA_MODULES="$PWD"
west build -d build -t run
```

A successful boot contains:

```text
CANopen stack initialized
```

Run the Zephyr sample test:

```shell
west twister -T samples/canopennode -p native_sim/native/64 --inline-logs
```

The CANopenNode submodule also contains standalone state-machine and mode tests:

```shell
make -C CANopenNode/test test
```

## Current limitations

- Object-dictionary persistence is disabled while `CONFIG_CANOPENNODE_CIA402=y` because the Zephyr
  storage adapter still targets the legacy generated-OD layout.

- CANopen program download is also disabled with CiA 402 until that adapter is ported to the v4 OD
  extension API.

- The generic state machine does not replace hardware safety functions. Overcurrent, overvoltage,
  safe-torque-off, watchdog, position-limit, and emergency-stop behavior must be enforced by the
  motor-control and safety layers independently of CANopen communication.

## Integration behavior added in this version

- Hardware-filter exhaustion now falls back to software CAN-ID dispatch.

- CSP, CSV, and CST have a dedicated synchronous processing entry point and are no longer executed
  from the asynchronous main loop.

- The Zephyr SYNC path processes RPDO, cyclic motion, and TPDO in deterministic order.

- CAN shutdown marks the CANopen module non-normal before removing filters.

- The sample serializes stack initialization, processing, communication reset, and deletion with
  the OD mutex.

- Builds with a CiA 402-capable OD remain valid when `CONFIG_CANOPENNODE_CIA402=n`.

- Tests cover profile behavior, faults, limits, CSP, CSV, CST, NMT gating, and the Mafratech SDO
  compatibility option.
