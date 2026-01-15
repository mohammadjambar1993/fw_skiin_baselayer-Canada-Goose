PROJECT_NAME     := udw_fw_afib
TARGETS          := udw_fw_afib
OUTPUT_DIRECTORY := build
HEXAPPPATH		 := $(OUTPUT_DIRECTORY)/udw_fw_afib.hex
HEXAPPSDPATH	 := $(OUTPUT_DIRECTORY)/udw_app_sd.hex
HEXBOOTPATH		 := dep/bootloader/bootloader_myant.hex
HEXBOOTSETTING	 := dep/bootloader/bootsettings.hex
DFUPATH			 := $(OUTPUT_DIRECTORY)/dfupack.zip

#DEBUGGER_SN      := 682959239
#DEBUGGER_SN      := 682918043
#DEBUGGER_SN      := 682822972
#DEBUGGER_SN      := 682224844

#DEBUGGER_SN	  := 682887267
#DEBUGGER_SN	  := 682270299
#DEBUGGER_SN	  := 682932127
#DEBUGGER_SN      := 682963104
#DEBUGGER_SN	  := 682056633
#DEBUGGER_SN      := 682947707
#DEBUGGER_SN      := 685327281
DEBUGGER_SN      := 1050612771
#DEBUGGER_SN      := 685302286


SDK_ROOT := ./sdk
PROJ_DIR := ./src
SOFTDEVICEPATH := $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex

$(OUTPUT_DIRECTORY)/udw_fw_afib.out: \
  LINKER_SCRIPT  := ./lnk/linker_xgaudw.ld

# Source files common to all targets
 
# SRC_FILES += $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52.S
SRC_FILES += $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52833.S
SRC_FILES += $(PROJ_DIR)/ble_evt.c
SRC_FILES += $(PROJ_DIR)/adc_ctrl.c
SRC_FILES += $(PROJ_DIR)/ble_rpc.c
SRC_FILES += $(PROJ_DIR)/ble_commands.c
SRC_FILES += $(PROJ_DIR)/cli/cli.c
SRC_FILES += $(PROJ_DIR)/cli/cli_mx25r64.c
SRC_FILES += $(PROJ_DIR)/cli/uartshell.c
SRC_FILES += $(PROJ_DIR)/cmd_protocol.c
SRC_FILES += $(PROJ_DIR)/cmd_uicr.c
SRC_FILES += $(PROJ_DIR)/drivers/bmi160.c
SRC_FILES += $(PROJ_DIR)/drivers/tps65987.c
SRC_FILES += $(PROJ_DIR)/drivers/mx25r6435.c
SRC_FILES += $(PROJ_DIR)/gtk_spi.c
SRC_FILES += $(PROJ_DIR)/hal/hal_ble_nrf52.c
SRC_FILES += $(PROJ_DIR)/hal/hal_clock_nrf52.c
SRC_FILES += $(PROJ_DIR)/hal/hal_config_nrf52.c
SRC_FILES += $(PROJ_DIR)/hal/hal_gpio_nrf52.c
SRC_FILES += $(PROJ_DIR)/hal/hal_i2c_nrf52.c
SRC_FILES += $(PROJ_DIR)/hal/hal_spi_nrf52.c
SRC_FILES += $(PROJ_DIR)/hal/hal_uart_nrf52.c
SRC_FILES += $(PROJ_DIR)/main.c
SRC_FILES += $(PROJ_DIR)/heat_ctrl.c
SRC_FILES += $(PROJ_DIR)/temperature_ads.c
SRC_FILES += $(PROJ_DIR)/drivers/ADS1220.c
SRC_FILES += $(PROJ_DIR)/bandcfg.c
SRC_FILES += $(PROJ_DIR)/pmic.c
SRC_FILES += $(PROJ_DIR)/system.c
SRC_FILES += $(PROJ_DIR)/diagnostic.c
SRC_FILES += $(PROJ_DIR)/drivers/ina231.c
SRC_FILES += $(PROJ_DIR)/tskctrl.c
SRC_FILES += $(PROJ_DIR)/util/evthandlers.c
SRC_FILES += $(PROJ_DIR)/util/logger.c
SRC_FILES += $(PROJ_DIR)/util/logio.c
SRC_FILES += $(PROJ_DIR)/util/maf.c
SRC_FILES += $(PROJ_DIR)/util/mya_str.c
SRC_FILES += $(PROJ_DIR)/util/mya_util.c
SRC_FILES += $(PROJ_DIR)/util/myaqueue.c
SRC_FILES += $(PROJ_DIR)/util/ringbuff.c
SRC_FILES += $(PROJ_DIR)/wdt.c
SRC_FILES += $(PROJ_DIR)/heater.c
SRC_FILES += $(PROJ_DIR)/soft_pwm.c
SRC_FILES += $(PROJ_DIR)/memory.c
SRC_FILES += $(PROJ_DIR)/algos/tempctl_pi1.c
SRC_FILES += $(SDK_ROOT)/components/ble/ble_advertising/ble_advertising.c
SRC_FILES += $(SDK_ROOT)/components/ble/ble_services/ble_bas/ble_bas.c
SRC_FILES += $(SDK_ROOT)/components/ble/ble_services/ble_dis/ble_dis.c
SRC_FILES += $(SDK_ROOT)/components/ble/ble_services/ble_hrs/ble_hrs.c
SRC_FILES += $(SDK_ROOT)/components/ble/common/ble_advdata.c
SRC_FILES += $(SDK_ROOT)/components/ble/common/ble_conn_params.c
SRC_FILES += $(SDK_ROOT)/components/ble/common/ble_conn_state.c
SRC_FILES += $(SDK_ROOT)/components/ble/common/ble_srv_common.c
SRC_FILES += $(SDK_ROOT)/components/ble/nrf_ble_gatt/nrf_ble_gatt.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/auth_status_tracker.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/gatt_cache_manager.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/gatts_cache_manager.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/id_manager.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/nrf_ble_lesc.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/peer_data_storage.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/peer_database.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/peer_id.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/peer_manager.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/peer_manager_handler.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/pm_buffer.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/security_dispatcher.c
SRC_FILES += $(SDK_ROOT)/components/ble/peer_manager/security_manager.c
SRC_FILES += $(SDK_ROOT)/components/libraries/atomic/nrf_atomic.c
SRC_FILES += $(SDK_ROOT)/components/libraries/atomic_fifo/nrf_atfifo.c
SRC_FILES += $(SDK_ROOT)/components/libraries/atomic_flags/nrf_atflags.c
SRC_FILES += $(SDK_ROOT)/components/libraries/balloc/nrf_balloc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_init.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_rng.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_rng_mbedtls.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crc16/crc16.c
SRC_FILES += $(SDK_ROOT)/components/libraries/experimental_section_vars/nrf_section_iter.c
SRC_FILES += $(SDK_ROOT)/components/libraries/fds/fds.c
SRC_FILES += $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage.c
SRC_FILES += $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage_sd.c
SRC_FILES += $(SDK_ROOT)/components/libraries/hardfault/hardfault_implementation.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_backend_rtt.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_backend_serial.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_backend_uart.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_default_backends.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_frontend.c
SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_str_formatter.c
SRC_FILES += $(SDK_ROOT)/components/libraries/mem_manager/mem_manager.c
SRC_FILES += $(SDK_ROOT)/components/libraries/memobj/nrf_memobj.c
SRC_FILES += $(SDK_ROOT)/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c
SRC_FILES += $(SDK_ROOT)/components/libraries/queue/nrf_queue.c
SRC_FILES += $(SDK_ROOT)/components/libraries/ringbuf/nrf_ringbuf.c
SRC_FILES += $(SDK_ROOT)/components/libraries/scheduler/app_scheduler.c
SRC_FILES += $(SDK_ROOT)/components/libraries/strerror/nrf_strerror.c
SRC_FILES += $(SDK_ROOT)/components/libraries/timer/app_timer.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/nrf_assert.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/app_error.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/app_error_handler_gcc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/app_error_weak.c
SRC_FILES += $(SDK_ROOT)/components/libraries/util/app_util_platform.c
SRC_FILES += $(SDK_ROOT)/components/softdevice/common/nrf_sdh.c
SRC_FILES += $(SDK_ROOT)/components/softdevice/common/nrf_sdh_ble.c
SRC_FILES += $(SDK_ROOT)/components/softdevice/common/nrf_sdh_soc.c
SRC_FILES += $(SDK_ROOT)/external/fprintf/nrf_fprintf.c
SRC_FILES += $(SDK_ROOT)/external/fprintf/nrf_fprintf_format.c
SRC_FILES += $(SDK_ROOT)/external/freertos/source/croutine.c
SRC_FILES += $(SDK_ROOT)/external/freertos/source/event_groups.c
SRC_FILES += $(SDK_ROOT)/external/freertos/source/portable/MemMang/heap_3.c
SRC_FILES += $(SDK_ROOT)/external/freertos/source/list.c
SRC_FILES += $(SDK_ROOT)/external/freertos/source/queue.c
SRC_FILES += $(SDK_ROOT)/external/freertos/source/tasks.c
SRC_FILES += $(SDK_ROOT)/external/freertos/source/timers.c
SRC_FILES += $(SDK_ROOT)/external/freertos/portable/GCC/nrf52/port.c
SRC_FILES += $(SDK_ROOT)/external/freertos/portable/CMSIS/nrf52/port_cmsis.c
SRC_FILES += $(SDK_ROOT)/external/freertos/portable/CMSIS/nrf52/port_cmsis_systick.c
SRC_FILES += $(SDK_ROOT)/external/mbedtls/library/aes.c
SRC_FILES += $(SDK_ROOT)/external/mbedtls/library/platform_util.c
SRC_FILES += $(SDK_ROOT)/external/mbedtls/library/ctr_drbg.c
SRC_FILES += $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT.c
SRC_FILES += $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_printf.c
SRC_FILES += $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_Syscalls_GCC.c
SRC_FILES += $(SDK_ROOT)/external/utf_converter/utf.c
SRC_FILES += $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_spi.c
SRC_FILES += $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_twi.c
SRC_FILES += $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_uart.c
SRC_FILES += $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_clock.c
SRC_FILES += $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_rng.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_gpiote.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/prs/nrfx_prs.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_spi.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_spim.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_twim.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_uart.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_uarte.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_clock.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_power.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_rng.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_rtc.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_saadc.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_wdt.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bsp/bsp.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bsp/bsp_btn_ble.c
SRC_FILES += $(SDK_ROOT)/components/libraries/button/app_button.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aead.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes_shared.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecdh.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecdsa.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_eddsa.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_error.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_hash.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_hkdf.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_hmac.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_init.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_rng.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_shared.c
SRC_FILES += $(SDK_ROOT)/components/ble/nrf_ble_qwr/nrf_ble_qwr.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/oberon/oberon_backend_chacha_poly_aead.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/oberon/oberon_backend_ecc.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/oberon/oberon_backend_ecdh.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/oberon/oberon_backend_ecdsa.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/oberon/oberon_backend_eddsa.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/oberon/oberon_backend_hash.c
SRC_FILES += $(SDK_ROOT)/components/libraries/crypto/backend/oberon/oberon_backend_hmac.c
SRC_FILES += $(SDK_ROOT)/components/libraries/sensorsim/sensorsim.c
SRC_FILES += $(SDK_ROOT)/components/boards/boards.c
# SRC_FILES += $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52.c
SRC_FILES += $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52833.c
SRC_FILES += $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu.c
SRC_FILES += $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu_bonded.c
SRC_FILES += $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu_unbonded.c
SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_svci.c


# Include folders common to all targets
INC_FOLDERS += ./config
INC_FOLDERS += $(PROJ_DIR)/
INC_FOLDERS += $(PROJ_DIR)/cli
INC_FOLDERS += $(PROJ_DIR)/drivers
INC_FOLDERS += $(PROJ_DIR)/hal
INC_FOLDERS += $(PROJ_DIR)/util
INC_FOLDERS += $(SDK_ROOT)/
INC_FOLDERS += $(SDK_ROOT)/components
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_advertising
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_dtm
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_racp
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_ans_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_ancs_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_bas
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_bas_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_cscs
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_cts_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_dfu
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_dis
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_gls
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_hids
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_hrs
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_hrs_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_hts
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_ias
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_ias_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_lbs
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_lbs_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_lls
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_tps
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_rscs
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_rscs_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/common
INC_FOLDERS += $(SDK_ROOT)/components/ble/nrf_ble_gatt
INC_FOLDERS += $(SDK_ROOT)/components/ble/nrf_ble_qwr
INC_FOLDERS += $(SDK_ROOT)/components/ble/peer_manager
INC_FOLDERS += $(SDK_ROOT)/components/boards
# INC_FOLDERS += $(SDK_ROOT)/components/drivers_nrf/usbd
INC_FOLDERS += $(SDK_ROOT)/components/libraries/atomic
INC_FOLDERS += $(SDK_ROOT)/components/libraries/atomic_fifo
INC_FOLDERS += $(SDK_ROOT)/components/libraries/atomic_flags
INC_FOLDERS += $(SDK_ROOT)/components/libraries/balloc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/bsp
INC_FOLDERS += $(SDK_ROOT)/components/libraries/bootloader
INC_FOLDERS += $(SDK_ROOT)/components/libraries/bootloader/dfu
INC_FOLDERS += $(SDK_ROOT)/components/libraries/bootloader/ble_dfu

INC_FOLDERS += $(SDK_ROOT)/components/libraries/button
INC_FOLDERS += $(SDK_ROOT)/components/libraries/cli
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crc16
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crc32
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/cc310
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/cc310_bl
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/cifra
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/micro_ecc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/nrf_sw
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/oberon
INC_FOLDERS += $(SDK_ROOT)/components/libraries/crypto/backend/optiga
INC_FOLDERS += $(SDK_ROOT)/components/libraries/csense
INC_FOLDERS += $(SDK_ROOT)/components/libraries/csense_drv
INC_FOLDERS += $(SDK_ROOT)/components/libraries/delay
INC_FOLDERS += $(SDK_ROOT)/components/libraries/ecc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/experimental_section_vars
INC_FOLDERS += $(SDK_ROOT)/components/libraries/experimental_task_manager
INC_FOLDERS += $(SDK_ROOT)/components/libraries/fds
INC_FOLDERS += $(SDK_ROOT)/components/libraries/fstorage
INC_FOLDERS += $(SDK_ROOT)/components/libraries/gfx
INC_FOLDERS += $(SDK_ROOT)/components/libraries/gpiote
INC_FOLDERS += $(SDK_ROOT)/components/libraries/hardfault
INC_FOLDERS += $(SDK_ROOT)/components/libraries/hci
INC_FOLDERS += $(SDK_ROOT)/components/libraries/log
INC_FOLDERS += $(SDK_ROOT)/components/libraries/low_power_pwm
INC_FOLDERS += $(SDK_ROOT)/components/libraries/log/src
INC_FOLDERS += $(SDK_ROOT)/components/libraries/mem_manager
INC_FOLDERS += $(SDK_ROOT)/components/libraries/memobj
INC_FOLDERS += $(SDK_ROOT)/components/libraries/mpu
INC_FOLDERS += $(SDK_ROOT)/components/libraries/mutex
INC_FOLDERS += $(SDK_ROOT)/components/libraries/pwm
INC_FOLDERS += $(SDK_ROOT)/components/libraries/pwr_mgmt
INC_FOLDERS += $(SDK_ROOT)/components/libraries/queue
INC_FOLDERS += $(SDK_ROOT)/components/libraries/ringbuf
INC_FOLDERS += $(SDK_ROOT)/components/libraries/scheduler
INC_FOLDERS += $(SDK_ROOT)/components/libraries/sdcard
INC_FOLDERS += $(SDK_ROOT)/components/libraries/slip
INC_FOLDERS += $(SDK_ROOT)/components/libraries/sortlist
INC_FOLDERS += $(SDK_ROOT)/components/libraries/stack_guard
INC_FOLDERS += $(SDK_ROOT)/components/libraries/stack_info
INC_FOLDERS += $(SDK_ROOT)/components/libraries/strerror
INC_FOLDERS += $(SDK_ROOT)/components/libraries/svc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/timer
INC_FOLDERS += $(SDK_ROOT)/components/libraries/twi_mngr
INC_FOLDERS += $(SDK_ROOT)/components/libraries/twi_sensor
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/audio
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/cdc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/cdc/acm
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/hid
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/hid/generic
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/hid/kbd
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/hid/mouse
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/msc
INC_FOLDERS += $(SDK_ROOT)/components/libraries/util
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser/ble_oob_advdata_parser
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser/le_oob_rec_parser
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/ep_oob_rec
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/common
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/le_oob_rec
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/ac_rec
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/ble_oob_advdata
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/ble_pair_msg
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/hs_rec
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/conn_hand_parser/ac_rec_parser
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/connection_handover/ble_pair_lib
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/generic/message
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/generic/record
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/launchapp
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/parser/message
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/parser/record
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/text
INC_FOLDERS += $(SDK_ROOT)/components/nfc/ndef/uri
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t2t_lib
# INC_FOLDERS += $(SDK_ROOT)/components/nfc/t2t_lib/hal_t2t
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t2t_parser
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t4t_lib
# INC_FOLDERS += $(SDK_ROOT)/components/nfc/t4t_lib/hal_t4t
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t4t_parser/apdu
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t4t_parser/cc_file
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t4t_parser/hl_detection_procedure
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t4t_parser/tlv
#INC_FOLDERS += $(SDK_ROOT)/components/softdevice/s132/headers
#INC_FOLDERS += $(SDK_ROOT)/components/softdevice/s132/headers/nrf52
INC_FOLDERS += $(SDK_ROOT)/components/softdevice/s140/headers
INC_FOLDERS += $(SDK_ROOT)/components/softdevice/s140/headers/nrf52
INC_FOLDERS += $(SDK_ROOT)/components/toolchain/cmsis/include
INC_FOLDERS += $(SDK_ROOT)/components/softdevice/common
INC_FOLDERS += $(SDK_ROOT)/components/toolchain/cmsis/dsp
# INC_FOLDERS += $(SDK_ROOT)/components/toolchain/cmsis/dsp/lib
INC_FOLDERS += $(SDK_ROOT)/external/fprintf
INC_FOLDERS += $(SDK_ROOT)/external/freertos
INC_FOLDERS += $(SDK_ROOT)/external/freertos/config
INC_FOLDERS += $(SDK_ROOT)/external/freertos/portable
INC_FOLDERS += $(SDK_ROOT)/external/freertos/portable/CMSIS/nrf52
INC_FOLDERS += $(SDK_ROOT)/external/freertos/portable/GCC/nrf52
INC_FOLDERS += $(SDK_ROOT)/external/freertos/source/include
INC_FOLDERS += $(SDK_ROOT)/external/mbedtls/include
INC_FOLDERS += $(SDK_ROOT)/external/nrf_cc310/include
INC_FOLDERS += $(SDK_ROOT)/external/nrf_oberon
INC_FOLDERS += $(SDK_ROOT)/external/nrf_oberon/include
INC_FOLDERS += $(SDK_ROOT)/external/nrf_tls/mbedtls/nrf_crypto/config
INC_FOLDERS += $(SDK_ROOT)/external/segger_rtt
INC_FOLDERS += $(SDK_ROOT)/external/utf_converter
INC_FOLDERS += $(SDK_ROOT)/integration/nrfx
INC_FOLDERS += $(SDK_ROOT)/integration/nrfx/legacy
INC_FOLDERS += $(SDK_ROOT)/modules/nrfx
INC_FOLDERS += $(SDK_ROOT)/modules/nrfx/drivers/include
INC_FOLDERS += $(SDK_ROOT)/modules/nrfx/hal
INC_FOLDERS += $(SDK_ROOT)/modules/nrfx/mdk
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/experimental_gatts_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_db_discovery
INC_FOLDERS += $(SDK_ROOT)/components/libraries/usbd/class/cdc
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_nus_c
INC_FOLDERS += $(SDK_ROOT)/components/ble/peer_manager
INC_FOLDERS += $(SDK_ROOT)/components/libraries/sensorsim
INC_FOLDERS += $(SDK_ROOT)/components/libraries/spi_mngr
INC_FOLDERS += $(SDK_ROOT)/components/libraries/led_softblink
INC_FOLDERS += $(SDK_ROOT)/components/nfc/t4t_lib
INC_FOLDERS += $(SDK_ROOT)/components/ble/ble_services/ble_nus

# Libraries common to all targets
LIB_FILES += $(SDK_ROOT)/external/nrf_cc310/lib/cortex-m4/hard-float/libnrf_cc310_0.9.13.a
LIB_FILES += $(SDK_ROOT)/external/nrf_oberon/lib/cortex-m4/hard-float/liboberon_3.0.6.a

# Optimization flags
OPT = -O0 -g3

#flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
#CFLAGS += -DNRF52
#CFLAGS += -DNRF52832_XXAA
#CFLAGS += -DNRF_SD_BLE_API_VERSION=6
CFLAGS += -DGCC_CROSS_COMPILER
CFLAGS += -DNRF52833_XXAA
CFLAGS += -DNRF_SD_BLE_API_VERSION=7
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -DS140
CFLAGS += -DFLOAT_ABI_HARD
CFLAGS += -DBLE_STACK_SUPPORT_REQD
#increasing MTU size might need changes in heap and and the respective task stack memory
CFLAGS += -DMYANT_BLE_MTU=58
CFLAGS += -DBOARD_PCA10100
CFLAGS += -DARM_MATH_CM4
CFLAGS += -DSWI_DISABLE0
CFLAGS += -DuECC_ENABLE_VLI_API=0
CFLAGS += -DuECC_OPTIMIZATION_LEVEL=3
CFLAGS += -DuECC_SQUARE_FUNC=0
CFLAGS += -DuECC_SUPPORT_COMPRESSED_POINT=0
CFLAGS += -DuECC_VLI_NATIVE_LITTLE_ENDIAN=1
CFLAGS += -DNRF_DFU_SVCI_ENABLED
CFLAGS += -DNRF_DFU_TRANSPORT_BLE=1
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs --std=gnu99
CFLAGS += -Wall -O0 -g3
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin --short-enums

# C++ flags common to all targets
CXXFLAGS += $(OPT)

# Assembler flags common to all targets
ASMFLAGS += -x assembler-with-cpp
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -DBLE_STACK_SUPPORT_REQD
ASMFLAGS += -DBOARD_PCA10100
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DFLOAT_ABI_HARD
#ASMFLAGS += -DNRF52
ASMFLAGS += -DNRF52833_XXAA
ASMFLAGS += -DNRF52_PAN_74
ASMFLAGS += -DNRF_CRYPTO_MAX_INSTANCE_COUNT=1
ASMFLAGS += -DNRF_SD_BLE_API_VERSION=7
ASMFLAGS += -DS140
ASMFLAGS += -DSOFTDEVICE_PRESENT
ASMFLAGS += -DSWI_DISABLE0
ASMFLAGS += -DNRF_DFU_SVCI_ENABLED
ASMFLAGS += -DNRF_DFU_TRANSPORT_BLE=1

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs
LDFLAGS += -u _printf_float

udw_fw_afib: CFLAGS += -D__HEAP_SIZE=8192
udw_fw_afib: CFLAGS += -D__STACK_SIZE=8192
udw_fw_afib: ASMFLAGS += -D__HEAP_SIZE=8192
udw_fw_afib: ASMFLAGS += -D__STACK_SIZE=8192

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm


.PHONY: default help

# Default target - first one defined
default: udw_fw_afib

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo		udw_fw_afib
	@echo		flash_softdevice
	@echo		sdk_config - starting external tool for editing sdk_config.h
	@echo		flash      - flashing binary
	$(CC) --version

TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc


include $(TEMPLATE_PATH)/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: flash flash_softdevice erase

# Flash the program
flash: default
	@echo Flashing: D:/Myant/Myant_Firmware/build/udw_fw_afib.hex
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --program D:/Myant/Myant_Firmware/build/udw_fw_afib.hex --sectorerase
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --verify D:/Myant/Myant_Firmware/build/udw_fw_afib.hex
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset

# Flash softdevice
flash_softdevice:
	@echo Flashing: s140_nrf52_7.2.0_softdevice.hex
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --program $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectorerase
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --verify $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset

erase:
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --eraseall

reset:
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset	

recover:
	nrfjprog -s $(DEBUGGER_SN) -f NRF52 --recover
	
erase_sd_flash:
	@echo Erasing flash...
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --eraseall
	@echo Flashing softdevice...
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --program $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectorerase
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --verify $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex	
	@echo Flashing application...
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --program $(OUTPUT_DIRECTORY)/udw_fw_afib.hex --sectorerase
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --verify $(OUTPUT_DIRECTORY)/udw_fw_afib.hex
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset

write_encryption_key:
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x10001098 --val 0x33221100
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x10001094 --val 0x77665544
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x10001090 --val 0xBBAA9988
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x1000108C --val 0xFFEEDDCC
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset
	
write_secret_number:
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x100010A8 --val 0x33221100
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x100010A4 --val 0x77665544
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x100010A0 --val 0xBBAA9988
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x1000109C --val 0xFFEEDDCC
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset 

write_serial_number:
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x10001088 --val 0x3D6EBADE
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --memwr 0x10001084 --val 0x0000000B
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset 

NRFUTIL=./dep/image_builder/nrfutil.exe
KEYFILE=C:/Users/Test/Desktop/Bootloader/key/dfu_heat_myant_private.key
IMAGE_BUILDER=./dep/image_builder/building.py

gen_dfu_package:
	nrfutil pkg generate --hw-version 52 --application-version 1 --application $(HEXAPPPATH) --sd-req 0x100 --key-file $(KEYFILE) $(DFUPATH)
	
gen_encrypted_dfu_package:
	nrfutil pkg generate --hw-version 52 --application-version 1 --application $(HEXAPPPATH) --sd-req 0x100 --key-file $(KEYFILE) $(DFUPATH)
	nrfutil pkg display C:/Users/Test/Desktop/Heat_project/fw_skiin_baselayer/build/dfupack.zip > $(OUTPUT_DIRECTORY)/imagebuilder_dfupackage_info.txt
	python $(IMAGE_BUILDER) 
	
gen_dfu_appsd_package:
	nrfutil pkg generate --hw-version 52 --application-version 3 --application $(HEXAPPSDPATH) --sd-req $(SD_FW_ID) --key-file $(KEYFILE) $(DFUPATH)


#erase, flash softdevice, flash bootloader, generate and flash bootloader settings
flash_bootloader: erase flash_softdevice 	
	@echo Flashing bootloader hex: $(HEXBOOTPATH)
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --program $(HEXBOOTPATH)
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --verify $(HEXBOOTPATH)
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset
	@echo Flashing bootloader settings: $(HEXBOOTSETTING)
	$(NRFUTIL) settings generate --family NRF52 --application $(HEXAPPPATH) --application-version 3 --bootloader-version 2 --bl-settings-version 1 $(HEXBOOTSETTING)
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --program $(HEXBOOTSETTING) --sectorerase
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --verify $(HEXBOOTSETTING)
	nrfjprog -s $(DEBUGGER_SN) -f nrf52 --reset

merge_sd_app:
	mergehex --merge $(SOFTDEVICEPATH) $(HEXAPPPATH) --output $(HEXAPPSDPATH)

merge_boot_set:
	$(NRFUTIL) settings generate --family NRF52 --application $(HEXAPPPATH) --application-version 3 --bootloader-version 2 --bl-settings-version 1 $(HEXBOOTSETTING)
	mergehex --merge $(HEXBOOTPATH) $(HEXBOOTSETTING) --output $(OUTPUT_DIRECTORY)/udw_boot_set.hex

merge_sd_boot:
	mergehex --merge $(SOFTDEVICEPATH) $(OUTPUT_DIRECTORY)/udw_boot_set.hex --output $(OUTPUT_DIRECTORY)/udw_boot_sd.hex

merge_boot_sd_app:	
	mergehex --merge $(OUTPUT_DIRECTORY)/udw_app_sd.hex $(OUTPUT_DIRECTORY)/udw_boot_set.hex --output $(OUTPUT_DIRECTORY)/udw_fw.hex

SDK_CONFIG_FILE := ./config/sdk_config.h
CMSIS_CONFIG_TOOL := $(SDK_ROOT)/external_tools/cmsisconfig/CMSIS_Configuration_Wizard.jar
sdk_config:
	java -jar $(CMSIS_CONFIG_TOOL) $(SDK_CONFIG_FILE)
