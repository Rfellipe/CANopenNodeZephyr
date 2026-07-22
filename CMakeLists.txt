# SPDX-License-Identifier: Apache-2.0

if(CONFIG_CANOPENNODE)
  zephyr_library()

  zephyr_include_directories(
    CANopenNode
    CANopenNode/301
    include
  )

  zephyr_library_sources(
    CANopenNode/CANopen.c
    CANopenNode/301/CO_Emergency.c
    CANopenNode/301/CO_fifo.c
    CANopenNode/301/CO_HBconsumer.c
    CANopenNode/301/CO_NMT_Heartbeat.c
    CANopenNode/301/CO_Node_Guarding.c
    CANopenNode/301/CO_ODinterface.c
    CANopenNode/301/CO_PDO.c
    CANopenNode/301/CO_SDOclient.c
    CANopenNode/301/CO_SDOserver.c
    CANopenNode/301/CO_SYNC.c
    CANopenNode/301/CO_TIME.c
    CANopenNode/301/crc16-ccitt.c
    CANopenNode/303/CO_LEDs.c
    CANopenNode/304/CO_GFC.c
    CANopenNode/304/CO_SRDO.c
    CANopenNode/305/CO_LSSmaster.c
    CANopenNode/305/CO_LSSslave.c
    CANopenNode/309/CO_gateway_ascii.c
    CANopenNode/extra/CO_trace.c
    src/CO_driver.c
  )

  zephyr_library_sources_ifdef(CONFIG_CANOPENNODE_CIA402 CANopenNode/402/CO_CiA402.c)
  zephyr_library_sources_ifdef(CONFIG_CANOPENNODE_SYNC_THREAD src/canopen_sync.c)
  zephyr_library_sources_ifdef(CONFIG_CANOPENNODE_STORAGE src/canopen_storage.c)
  zephyr_library_sources_ifdef(CONFIG_CANOPENNODE_LEDS src/canopen_leds.c)
  zephyr_library_sources_ifdef(CONFIG_CANOPENNODE_PROGRAM_DOWNLOAD src/canopen_program.c)

endif()
