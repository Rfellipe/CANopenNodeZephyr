/*
 * Copyright (c) 2019 Vestas Wind Systems A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>

#include <canopennode.h>
#include <OD.h>

#define LOG_LEVEL CONFIG_CANOPENNODE_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

#define CAN_INTERFACE DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus))
#define CAN_BITRATE (DT_PROP_OR(DT_CHOSEN(zephyr_canbus), bitrate, \
					  DT_PROP_OR(DT_CHOSEN(zephyr_canbus), bus_speed, \
						     CONFIG_CAN_DEFAULT_BITRATE)) / 1000)

#define FIRST_HB_TIME_MS 500U
#define SDO_SERVER_TIMEOUT_MS 1000U
#define SDO_CLIENT_TIMEOUT_MS 500U
#define NMT_CONTROL (CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_ERR_ON_BUSOFF_HB)

static struct gpio_dt_spec led_green_gpio = GPIO_DT_SPEC_GET_OR(
		DT_ALIAS(green_led), gpios, {0});
static struct gpio_dt_spec led_red_gpio = GPIO_DT_SPEC_GET_OR(
		DT_ALIAS(red_led), gpios, {0});

CO_t *CO;

#if defined(CONFIG_CANOPENNODE_CIA402)
struct cia402_sim_drive {
	CO_CiA402_feedback_t feedback;
	CO_CiA402_motionConfig_t last_config;
	int8_t mode;
	bool_t voltage_enabled;
	bool_t switched_on;
	bool_t operation_enabled;
	bool_t quick_stop_active;
	bool_t halt_active;
};

static void cia402_store_config(struct cia402_sim_drive *drive,
				const CO_CiA402_motionConfig_t *config)
{
	if (config != NULL) {
		drive->last_config = *config;
	}
}

static bool_t cia402_set_enable_voltage(void *object, bool_t enable)
{
	struct cia402_sim_drive *drive = object;

	drive->voltage_enabled = enable;
	return true;
}

static bool_t cia402_set_switch_on(void *object, bool_t on)
{
	struct cia402_sim_drive *drive = object;

	drive->switched_on = on;
	return true;
}

static bool_t cia402_set_operation_enabled(void *object, bool_t enable)
{
	struct cia402_sim_drive *drive = object;

	drive->operation_enabled = enable;
	return true;
}

static bool_t cia402_set_quick_stop(void *object, bool_t quick_stop_active,
				    const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	cia402_store_config(drive, config);
	drive->quick_stop_active = quick_stop_active;
	return true;
}

static bool_t cia402_set_halt(void *object, bool_t halt_active,
			      const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	cia402_store_config(drive, config);
	drive->halt_active = halt_active;
	return true;
}

static bool_t cia402_set_mode(void *object, int8_t mode)
{
	struct cia402_sim_drive *drive = object;

	drive->mode = mode;
	return true;
}

static bool_t cia402_fault_reset(void *object)
{
	struct cia402_sim_drive *drive = object;

	drive->feedback.faultActive = false;
	drive->feedback.faultCode = CO_CIA402_ERR_NONE;
	return true;
}

static bool_t cia402_run_profile_position(void *object, int32_t target_position,
					  bool_t relative, bool_t override,
					  bool_t change_on_set_point,
					  const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	ARG_UNUSED(override);
	ARG_UNUSED(change_on_set_point);
	cia402_store_config(drive, config);

	if (relative) {
		drive->feedback.positionActualValue += target_position;
	} else {
		drive->feedback.positionActualValue = target_position;
	}

	drive->feedback.targetReached = true;
	drive->feedback.setPointAcknowledged = true;
	return true;
}

static bool_t cia402_run_profile_velocity(void *object, int32_t target_velocity,
					  const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	cia402_store_config(drive, config);
	drive->feedback.velocityActualValue = target_velocity;
	drive->feedback.targetReached = target_velocity == 0;
	return true;
}

static bool_t cia402_run_profile_torque(void *object, int16_t target_torque,
					const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	cia402_store_config(drive, config);
	drive->feedback.torqueActualValue = target_torque;
	drive->feedback.targetReached = target_torque == 0;
	return true;
}

static bool_t cia402_run_homing(void *object, int8_t homing_method,
				const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	ARG_UNUSED(homing_method);
	cia402_store_config(drive, config);
	drive->feedback.positionActualValue = 0;
	drive->feedback.homingAttained = true;
	drive->feedback.homingCompleted = true;
	drive->feedback.targetReached = true;
	return true;
}

static bool_t cia402_run_csp(void *object, int32_t target_position,
			     const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	cia402_store_config(drive, config);
	drive->feedback.positionActualValue = target_position;
	drive->feedback.targetReached = true;
	return true;
}

static bool_t cia402_run_csv(void *object, int32_t target_velocity,
			     const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	cia402_store_config(drive, config);
	drive->feedback.velocityActualValue = target_velocity;
	return true;
}

static bool_t cia402_run_cst(void *object, int16_t target_torque,
			     const CO_CiA402_motionConfig_t *config)
{
	struct cia402_sim_drive *drive = object;

	cia402_store_config(drive, config);
	drive->feedback.torqueActualValue = target_torque;
	return true;
}

static bool_t cia402_get_feedback(void *object, CO_CiA402_feedback_t *feedback)
{
	struct cia402_sim_drive *drive = object;

	*feedback = drive->feedback;
	return true;
}

static struct cia402_sim_drive cia402_drive;

static const CO_CiA402_hwInterface_t cia402_hw = {
	.setEnableVoltage = cia402_set_enable_voltage,
	.setSwitchOn = cia402_set_switch_on,
	.setOperationEnabled = cia402_set_operation_enabled,
	.setQuickStop = cia402_set_quick_stop,
	.setHalt = cia402_set_halt,
	.setMode = cia402_set_mode,
	.faultReset = cia402_fault_reset,
	.runProfilePosition = cia402_run_profile_position,
	.runProfileVelocity = cia402_run_profile_velocity,
	.runProfileTorque = cia402_run_profile_torque,
	.runHoming = cia402_run_homing,
	.runCyclicSynchronousPosition = cia402_run_csp,
	.runCyclicSynchronousVelocity = cia402_run_csv,
	.runCyclicSynchronousTorque = cia402_run_cst,
	.getFeedback = cia402_get_feedback,
};
#endif

static void led_callback(bool value, void *arg)
{
	struct gpio_dt_spec *led_gpio = arg;

	if (led_gpio == NULL || led_gpio->port == NULL) {
		return;
	}

	gpio_pin_set_dt(led_gpio, value);
}

static void configure_led_gpio(struct gpio_dt_spec *led_gpio, const char *name)
{
	int err;

	if (led_gpio->port == NULL) {
		LOG_INF("%s LED not available", name);
		return;
	}

	if (!gpio_is_ready_dt(led_gpio)) {
		LOG_ERR("%s LED device not ready", name);
		led_gpio->port = NULL;
		return;
	}

	err = gpio_pin_configure_dt(led_gpio, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("failed to configure %s LED gpio: %d", name, err);
		led_gpio->port = NULL;
	}
}

static void configure_leds(CO_LEDs_t *leds)
{
	configure_led_gpio(&led_green_gpio, "green");
	configure_led_gpio(&led_red_gpio, "red");
	canopen_leds_init(leds, led_callback, &led_green_gpio,
			  led_callback, &led_red_gpio);
}

static bool canopen_init_stack(struct canopen_context *can, uint8_t node_id,
			       uint16_t bitrate)
{
	CO_ReturnError_t err;
	uint32_t err_info = 0;

	CO_CANsetConfigurationMode(can);
	CO_CANmodule_disable(CO->CANmodule);

	err = CO_CANinit(CO, can, bitrate);
	if (err != CO_ERROR_NO) {
		LOG_ERR("CAN initialization failed: %d", err);
		return false;
	}

	err = CO_CANopenInit(CO, NULL, NULL, OD, NULL, NMT_CONTROL,
			    FIRST_HB_TIME_MS, SDO_SERVER_TIMEOUT_MS,
			    SDO_CLIENT_TIMEOUT_MS, false, node_id, &err_info);
	if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
		if (err == CO_ERROR_OD_PARAMETERS) {
			LOG_ERR("CANopen OD parameter error at 0x%04x", err_info);
		} else {
			LOG_ERR("CANopen initialization failed: %d", err);
		}
		return false;
	}

	if (CO->CANmodule != NULL) {
		CO->CANmodule->em = CO->em;
	}

	err = CO_CANopenInitPDO(CO, CO->em, OD, node_id, &err_info);
	if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
		if (err == CO_ERROR_OD_PARAMETERS) {
			LOG_ERR("PDO OD parameter error at 0x%04x", err_info);
		} else {
			LOG_ERR("PDO initialization failed: %d", err);
		}
		return false;
	}

#if defined(CONFIG_CANOPENNODE_CIA402)
	CO_CANopenInitCiA402Hw(CO, &cia402_drive, &cia402_hw);
#endif

	configure_leds(CO->LEDs);
	CO_CANsetNormalMode(CO->CANmodule);

	return true;
}

int main(void)
{
	struct canopen_context can = {
		.dev = CAN_INTERFACE,
	};
	CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
	uint32_t heap_memory_used = 0;
	uint32_t elapsed_us = 0U;
	int64_t timestamp;

	if (!device_is_ready(can.dev)) {
		LOG_ERR("CAN interface not ready");
		return 0;
	}

	CO = CO_new(NULL, &heap_memory_used);
	if (CO == NULL) {
		LOG_ERR("failed to allocate CANopen objects");
		return 0;
	}

	LOG_INF("allocated %u bytes for CANopen objects", heap_memory_used);

	while (reset != CO_RESET_APP) {
		bool initialized;

		CO_LOCK_OD(NULL);
		initialized = canopen_init_stack(&can, CONFIG_CANOPEN_NODE_ID,
					   CAN_BITRATE);
		CO_UNLOCK_OD(NULL);
		if (!initialized) {
			CO_LOCK_OD(NULL);
			CO_delete(CO);
			CO = NULL;
			CO_UNLOCK_OD(NULL);
			return 0;
		}

		LOG_INF("CANopen stack initialized");
		reset = CO_RESET_NOT;
		elapsed_us = 0U;

		while (reset == CO_RESET_NOT) {
			uint32_t timer_next_us = 1000U;

			timestamp = k_uptime_get();
			CO_LOCK_OD(NULL);
			reset = CO_process(CO, false, elapsed_us, &timer_next_us);
			CO_UNLOCK_OD(NULL);

			if (reset != CO_RESET_NOT) {
				break;
			}

			if (!IS_ENABLED(CONFIG_CANOPENNODE_SYNC_THREAD)) {
				CO_LOCK_OD(NULL);
				bool_t sync = CO_process_SYNC(CO, elapsed_us, NULL);

				CO_process_RPDO(CO, sync, elapsed_us, NULL);
#if defined(CONFIG_CANOPENNODE_CIA402)
				if (sync && CO->NMT != NULL) {
					CO_CiA402_processSync(
						CO->CiA402,
						CO->NMT->operatingState == CO_NMT_OPERATIONAL,
						elapsed_us);
				}
#endif
				CO_process_TPDO(CO, sync, elapsed_us, NULL);
				CO_UNLOCK_OD(NULL);
			}

			if (timer_next_us > 0U) {
				k_sleep(K_USEC(timer_next_us));
				elapsed_us = (uint32_t)k_uptime_delta(&timestamp) *
					     USEC_PER_MSEC;
			} else {
				elapsed_us = 0U;
			}
		}

		if (reset == CO_RESET_COMM) {
			LOG_INF("resetting CANopen communication");
		}
	}

	LOG_INF("resetting device");

	CO_CANmodule_disable(CO->CANmodule);
	CO_LOCK_OD(NULL);
	CO_delete(CO);
	CO = NULL;
	CO_UNLOCK_OD(NULL);
	sys_reboot(SYS_REBOOT_COLD);

	return 0;
}
