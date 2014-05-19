/**************************************************************************
*
* Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include "config.h"
#include "server.h"
#include "address.h"
#include "bacdef.h"
#include "handlers.h"
#include "client.h"
#include "dlenv.h"
#include "bacdcode.h"
#include "npdu.h"
#include "apdu.h"
#include "iam.h"
#include "tsm.h"
#include "device.h"
#include "bacfile.h"
#include "datalink.h"
#include "dcc.h"
#include "getevent.h"
#include "net.h"
#include "txbuf.h"
#include "lc.h"
#include "version.h"
/* include the device object */
#include "device.h"
#include "trendlog.h"
#if defined(INTRINSIC_REPORTING)
#include "nc.h"
#endif /* defined(INTRINSIC_REPORTING) */
#if defined(BACFILE)
#include "bacfile.h"
#endif /* defined(BACFILE) */
#include "ucix.h"
#include "ai.h"
#include "ao.h"
#include "av.h"
#include "bi.h"
#include "bo.h"
#include "bv.h"
#include "msi.h"
#include "mso.h"
#include "msv.h"

/** @file server/main.c  Example server application using the BACnet Stack. */

/* (Doxygen note: The next two lines pull all the following Javadoc
 *  into the ServerDemo module.) */
/** @addtogroup ServerDemo */
/*@{*/

/** Buffer used for receiving */
static uint8_t Rx_Buf[MAX_MPDU] = { 0 };

static time_t uci_Update(
	time_t ucimodtime,
	BACNET_OBJECT_TYPE update_object_type,
	int ucirewrite
	)
{
	float val_f, pval_f;
	int val_i, pval_i;
	char *section;
	char *type;
	time_t chk_mtime = 0;
	struct uci_context *ctx;
	int uci_idx = 0;
	/* update Value from uci */
	section = NULL;
	type = NULL;
	if (update_object_type == OBJECT_ANALOG_INPUT) {
		section = "bacnet_ai";
		type = "ai";
	} else if (update_object_type == OBJECT_ANALOG_OUTPUT) {
		section = "bacnet_ao";
		type = "ao";
	} else if (update_object_type == OBJECT_ANALOG_VALUE) {
		section = "bacnet_av";
		type = "av";
	} else if (update_object_type == OBJECT_BINARY_INPUT) {
		section = "bacnet_bi";
		type = "bi";
	} else if (update_object_type == OBJECT_BINARY_OUTPUT) {
		section = "bacnet_bo";
		type = "bo";
	} else if (update_object_type == OBJECT_BINARY_VALUE) {
		section = "bacnet_bv";
		type = "bv";
//	} else if (update_object_type == OBJECT_MULTI_STATE_INPUT) {
//		section = "bacnet_mi";
//		type = "mi";
	} else if (update_object_type == OBJECT_MULTI_STATE_OUTPUT) {
		section = "bacnet_mo";
		type = "mo";
	} else if (update_object_type == OBJECT_MULTI_STATE_VALUE) {
		section = "bacnet_mv";
		type = "mv";
	} else {
		return 0;
	}
	if ( ucirewrite == 0) {
		chk_mtime = ucimodtime;
#if PRINT_ENABLED
		printf("rewrite %s\n", type);
#endif
	} else {
		chk_mtime = check_uci_update(section, ucimodtime);
	}
	if(chk_mtime != 0) {
		sleep(1);
		ucimodtime = chk_mtime;
#if PRINT_ENABLED
		printf("Config changed, reloading %s\n",section);
#endif
		ctx = ucix_init(section);
		struct uci_itr_ctx itr;
		value_tuple_t *cur;
		itr.list = NULL;
		itr.section = section;
		itr.ctx = ctx;
		ucix_for_each_section_type(ctx, section, type,
			(void *)load_value, &itr);
		for( cur = itr.list; cur; cur = cur->next ) {
#if PRINT_ENABLED
			printf("section %s idx %s \n", section, cur->idx);
#endif
			uci_idx = atoi(cur->idx);
#if PRINT_ENABLED
			printf("idx %s ",cur->idx);
			printf("value %s\n",cur->value);
#endif
/* update Analog Input from uci */
			if (update_object_type == OBJECT_ANALOG_INPUT) {
					val_f = strtof(cur->value,NULL);
					pval_f = Analog_Input_Present_Value(uci_idx);
					if ( val_f != pval_f ) {
						Analog_Input_Present_Value_Set(uci_idx,val_f,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Analog_Input_Out_Of_Service(uci_idx))
							Analog_Input_Out_Of_Service_Set(uci_idx,0);
						if (Analog_Input_Reliability(uci_idx))
							Analog_Input_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Analog_Input_Out_Of_Service_Set(uci_idx,1);
						Analog_Input_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
/* update Analog Output from uci */
			} else if (update_object_type == OBJECT_ANALOG_OUTPUT) {
					val_f = strtof(cur->value,NULL);
					pval_f = Analog_Output_Present_Value(uci_idx);
					if ( val_f != pval_f ) {
						Analog_Output_Present_Value_Set(uci_idx,val_f,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Analog_Output_Out_Of_Service(uci_idx))
							Analog_Output_Out_Of_Service_Set(uci_idx,0);
						if (Analog_Output_Reliability(uci_idx))
							Analog_Output_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Analog_Output_Out_Of_Service_Set(uci_idx,1);
						Analog_Output_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
/* update Analog Value from uci */
			} else if (update_object_type == OBJECT_ANALOG_VALUE) {
					val_f = strtof(cur->value,NULL);
					pval_f = Analog_Value_Present_Value(uci_idx);
					if ( val_f != pval_f ) {
						Analog_Value_Present_Value_Set(uci_idx,val_f,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Analog_Value_Out_Of_Service(uci_idx))
							Analog_Value_Out_Of_Service_Set(uci_idx,0);
						if (Analog_Value_Reliability(uci_idx))
							Analog_Value_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Analog_Value_Out_Of_Service_Set(uci_idx,1);
						Analog_Value_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
/* update Binary Input from uci */
			} else if (update_object_type == OBJECT_BINARY_INPUT) {
					val_i = atoi(cur->value);
					pval_i = Binary_Input_Present_Value(uci_idx);
					if ( val_i != pval_i ) {
						Binary_Input_Present_Value_Set(uci_idx,val_i,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Binary_Input_Out_Of_Service(uci_idx))
							Binary_Input_Out_Of_Service_Set(uci_idx,0);
						if (Binary_Input_Reliability(uci_idx))
							Binary_Input_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Binary_Input_Out_Of_Service_Set(uci_idx,1);
						Binary_Input_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
/* update Binary Output from uci */
			} else if (update_object_type == OBJECT_BINARY_OUTPUT) {
					val_i = atoi(cur->value);
					pval_i = Binary_Output_Present_Value(uci_idx);
					if ( val_i != pval_i ) {
						Binary_Output_Present_Value_Set(uci_idx,val_i,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Binary_Output_Out_Of_Service(uci_idx))
							Binary_Output_Out_Of_Service_Set(uci_idx,0);
						if (Binary_Output_Reliability(uci_idx))
							Binary_Output_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Binary_Output_Out_Of_Service_Set(uci_idx,1);
						Binary_Output_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
/* update Binary Value from uci */
			} else if (update_object_type == OBJECT_BINARY_VALUE) {
					val_i = atoi(cur->value);
					pval_i = Analog_Value_Present_Value(uci_idx);
					if ( val_i != pval_i ) {
						Binary_Value_Present_Value_Set(uci_idx,val_i,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Binary_Value_Out_Of_Service(uci_idx))
							Binary_Value_Out_Of_Service_Set(uci_idx,0);
						if (Binary_Value_Reliability(uci_idx))
							Binary_Value_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Binary_Value_Out_Of_Service_Set(uci_idx,1);
						Binary_Value_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
/* update Multistate Input from uci */
/*			} else if (update_object_type == OBJECT_MULTI_STATE_INPUT) {
					val_i = atoi(cur->value);
					pval_i = Multistate_Input_Present_Value(uci_idx);
					if ( val_i != pval_i ) {
						Multistate_Input_Present_Value_Set(uci_idx,val_i,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Multistate_Input_Out_Of_Service(uci_idx))
							Multistate_Input_Out_Of_Service_Set(uci_idx,0);
						if (Multistate_Input_Reliability(uci_idx))
							Multistate_Input_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Multistate_Input_Out_Of_Service_Set(uci_idx,1);
						Multistate_Input_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}*/
/* update Multistate Output from uci */
			} else if (update_object_type == OBJECT_MULTI_STATE_OUTPUT) {
					val_i = atoi(cur->value);
					pval_i = Multistate_Output_Present_Value(uci_idx);
					if ( val_i != pval_i ) {
						Multistate_Output_Present_Value_Set(uci_idx,val_i,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Multistate_Output_Out_Of_Service(uci_idx))
							Multistate_Output_Out_Of_Service_Set(uci_idx,0);
						if (Multistate_Output_Reliability(uci_idx))
							Multistate_Output_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Multistate_Output_Out_Of_Service_Set(uci_idx,1);
						Multistate_Output_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
/* update Multistate Value from uci */
			} else if (update_object_type == OBJECT_MULTI_STATE_VALUE) {
					val_i = atoi(cur->value);
					pval_i = Multistate_Value_Present_Value(uci_idx);
					if ( val_i != pval_i ) {
						Multistate_Value_Present_Value_Set(uci_idx,val_i,16);
					}
					if (cur->Out_Of_Service == 0) {
						if (Multistate_Value_Out_Of_Service(uci_idx))
							Multistate_Value_Out_Of_Service_Set(uci_idx,0);
						if (Multistate_Value_Reliability(uci_idx))
							Multistate_Value_Reliability_Set(uci_idx,
								RELIABILITY_NO_FAULT_DETECTED);
					} else {
#if PRINT_ENABLED
						printf("idx %s ",cur->idx);
						printf("Out_Of_Service\n");
#endif
						Multistate_Value_Out_Of_Service_Set(uci_idx,1);
						Multistate_Value_Reliability_Set(uci_idx,
							RELIABILITY_COMMUNICATION_FAILURE);
					}
			}
		}
		ucix_cleanup(ctx);
	}
	/* update end */
	return ucimodtime;
}


/** Initialize the handlers we will utilize.
 * @see Device_Init, apdu_set_unconfirmed_handler, apdu_set_confirmed_handler
 */
static void Init_Service_Handlers(
    void)
{
    Device_Init(NULL);
    /* we need to handle who-is to support dynamic device binding */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_bind);
    /* set the handler for all the services we don't implement */
    /* It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler
        (handler_unrecognized_service);
    /* Set the handlers for any confirmed services that we support. */
    /* We must implement read property - it's required! */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY,
        handler_read_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
        handler_read_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROPERTY,
        handler_write_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE,
        handler_write_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_RANGE,
        handler_read_range);
#if defined(BACFILE)
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_ATOMIC_READ_FILE,
        handler_atomic_read_file);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_ATOMIC_WRITE_FILE,
        handler_atomic_write_file);
#endif
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_REINITIALIZE_DEVICE,
        handler_reinitialize_device);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_UTC_TIME_SYNCHRONIZATION,
        handler_timesync_utc);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_TIME_SYNCHRONIZATION,
        handler_timesync);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV,
        handler_cov_subscribe);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_COV_NOTIFICATION,
        handler_ucov_notification);
    /* handle communication so we can shutup when asked */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);
    /* handle the data coming back from private requests */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_PRIVATE_TRANSFER,
        handler_unconfirmed_private_transfer);
#if defined(INTRINSIC_REPORTING)
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_ACKNOWLEDGE_ALARM,
        handler_alarm_ack);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_GET_EVENT_INFORMATION,
        handler_get_event_information);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_GET_ALARM_SUMMARY,
        handler_get_alarm_summary);
#endif /* defined(INTRINSIC_REPORTING) */
}

/** Main function of server demo.
 *
 * @see Device_Set_Object_Instance_Number, dlenv_init, Send_I_Am,
 *      datalink_receive, npdu_handler,
 *      dcc_timer_seconds, bvlc_maintenance_timer,
 *      Load_Control_State_Machine_Handler, handler_cov_task,
 *      tsm_timer_milliseconds
 *
 * @param argc [in] Arg count.
 * @param argv [in] Takes one argument: the Device Instance #.
 * @return 0 on success.
 */
int main(
    int argc,
    char *argv[])
{
    BACNET_ADDRESS src = {
        0
    };  /* address where message came from */
    uint16_t pdu_len = 0;
    unsigned timeout = 1;       /* milliseconds */
    time_t last_seconds = 0;
    time_t current_seconds = 0;
    uint32_t elapsed_seconds = 0;
    uint32_t elapsed_milliseconds = 0;
    uint32_t address_binding_tmr = 0;
    uint32_t recipient_scan_tmr = 0;
    int uci_id = 0;
    struct uci_context *ctx;
    time_t ucimodtime_bacnet_ai = 0;
    time_t ucimodtime_bacnet_ao = 0;
    time_t ucimodtime_bacnet_av = 0;
    time_t ucimodtime_bacnet_bi = 0;
    time_t ucimodtime_bacnet_bo = 0;
    time_t ucimodtime_bacnet_bv = 0;
    time_t ucimodtime_bacnet_mi = 0;
    time_t ucimodtime_bacnet_mo = 0;
    time_t ucimodtime_bacnet_mv = 0;
    char *pEnv = NULL;
    int rewrite;
    BACNET_OBJECT_TYPE u_object_type;

    pEnv = getenv("UCI_SECTION");
    ctx = ucix_init("bacnet_dev");
#if PRINT_ENABLED
    if(!ctx)
        fprintf(stderr,  "Failed to load config file bacnet_dev\n");
#endif
    uci_id = ucix_get_option_int(ctx, "bacnet_dev", pEnv, "id", 0);
    if (uci_id != 0) {
        Device_Set_Object_Instance_Number(uci_id);
    } else {
        /* allow the device ID to be set */
        if (argc > 1)
            Device_Set_Object_Instance_Number(strtol(argv[1], NULL, 0));
    }
    if(ctx)
        ucix_cleanup(ctx);

#if PRINT_ENABLED
    printf("BACnet Server with uci\n" "BACnet Stack Version %s\n"
        "BACnet Device ID: %u\n" "Max APDU: %d\n", BACnet_Version,
        Device_Object_Instance_Number(), MAX_APDU);
#endif
    /* load any static address bindings to show up
       in our device bindings list */
    address_init();
    Init_Service_Handlers();
    dlenv_init();
    atexit(datalink_cleanup);
    /* configure the timeout values */
    last_seconds = time(NULL);
    /* broadcast an I-Am on startup */
    Send_I_Am(&Handler_Transmit_Buffer[0]);
    /* loop forever */
    for (;;) {
        /* input */
        current_seconds = time(NULL);

        /* returns 0 bytes on timeout */
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);

        /* process */
        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }
        /* at least one second has passed */
        elapsed_seconds = (uint32_t) (current_seconds - last_seconds);
        if (elapsed_seconds) {
            last_seconds = current_seconds;
            dcc_timer_seconds(elapsed_seconds);
#if defined(BACDL_BIP) && BBMD_ENABLED
            bvlc_maintenance_timer(elapsed_seconds);
#endif
            dlenv_maintenance_timer(elapsed_seconds);
            Load_Control_State_Machine_Handler();
            elapsed_milliseconds = elapsed_seconds * 1000;
            handler_cov_timer_seconds(elapsed_seconds);
            tsm_timer_milliseconds(elapsed_milliseconds);
            trend_log_timer(elapsed_seconds);
#if defined(INTRINSIC_REPORTING)
            Device_local_reporting();
#endif
        }
        handler_cov_task();
        /* scan cache address */
        address_binding_tmr += elapsed_seconds;
        if (address_binding_tmr >= 60) {
            address_cache_timer(address_binding_tmr);
            address_binding_tmr = 0;
        }
#if defined(INTRINSIC_REPORTING)
        /* try to find addresses of recipients */
        recipient_scan_tmr += elapsed_seconds;
        if (recipient_scan_tmr >= NC_RESCAN_RECIPIENTS_SECS) {
            Notification_Class_find_recipient();
            recipient_scan_tmr = 0;
        }
#endif
//#if false
        /* output */
        rewrite++;
        if (rewrite>10000) {
            rewrite=0;
            printf("rewrite %i\n", rewrite);
        }
        /* update Analog Input from uci */
        u_object_type = OBJECT_ANALOG_INPUT;
        ucimodtime_bacnet_ai = uci_Update(ucimodtime_bacnet_ai,u_object_type,rewrite);
        /* update Analog Output from uci */
        u_object_type = OBJECT_ANALOG_OUTPUT;
        ucimodtime_bacnet_ao = uci_Update(ucimodtime_bacnet_ao,u_object_type,rewrite);
        /* update Analog Value from uci */
        u_object_type = OBJECT_ANALOG_VALUE;
        ucimodtime_bacnet_av = uci_Update(ucimodtime_bacnet_av,u_object_type,rewrite);
        /* update Binary Input from uci */
        u_object_type = OBJECT_BINARY_INPUT;
        ucimodtime_bacnet_bi = uci_Update(ucimodtime_bacnet_bi,u_object_type,rewrite);
        /* update Binary Output from uci */
        u_object_type = OBJECT_BINARY_OUTPUT;
        ucimodtime_bacnet_bo = uci_Update(ucimodtime_bacnet_bo,u_object_type,rewrite);
        /* update Binary Output from uci */
        u_object_type = OBJECT_BINARY_VALUE;
        ucimodtime_bacnet_bv = uci_Update(ucimodtime_bacnet_bv,u_object_type,rewrite);
        /* update Multistate Input from uci */
        u_object_type = OBJECT_MULTI_STATE_INPUT;
        ucimodtime_bacnet_mi = uci_Update(ucimodtime_bacnet_mi,u_object_type,rewrite);
        /* update Multistate Output from uci */
        u_object_type = OBJECT_MULTI_STATE_OUTPUT;
        ucimodtime_bacnet_mo = uci_Update(ucimodtime_bacnet_mo,u_object_type,rewrite);
        /* update Multistate Value from uci */
        u_object_type = OBJECT_MULTI_STATE_VALUE;
        ucimodtime_bacnet_mv = uci_Update(ucimodtime_bacnet_mv,u_object_type,rewrite);
//#endif
        /* blink LEDs, Turn on or off outputs, etc */
    }

    return 0;
}

/* @} */

/* End group ServerDemo */
