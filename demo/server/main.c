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
#if defined(BACFILE)
#include "bacfile.h"
#endif
#include "datalink.h"
#include "dcc.h"
#include "filename.h"
#include "getevent.h"
#include "net.h"
#include "txbuf.h"
#if defined(LC)
#include "lc.h"
#endif

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

#if defined(BAC_UCI)
#include "ucix.h"

#if defined(AI)
#include "ai.h"
#endif

#if defined(AO)
#include "ao.h"
#endif

#if defined(AV)
#include "av.h"
#endif

#if defined(BI)
#include "bi.h"
#endif

#if defined(BO)
#include "bo.h"
#endif

#if defined(BV)
#include "bv.h"
#endif

#if defined(MSI)
#include "msi.h"
#endif

#if defined(MSO)
#include "mso.h"
#endif

#if defined(MSV)
#include "msv.h"
#endif

#endif /* defined(BAC_UCI) */


/** @file server/main.c  Example server application using the BACnet Stack. */

/* (Doxygen note: The next two lines pull all the following Javadoc
 *  into the ServerDemo module.) */
/** @addtogroup ServerDemo */
/*@{*/

/** Buffer used for receiving */
static uint8_t Rx_Buf[MAX_MPDU] = { 0 };

#if defined(BAC_UCI)
#if defined(AI) || defined(AO) || defined(AV) || defined(BI) || defined(BO) || defined(BV) || defined(MSI) || defined(MSO) || defined(MSV)
static time_t uci_Update(
	time_t ucimodtime,
	BACNET_OBJECT_TYPE update_object_type,
	int ucirewrite
	)
{
	char *section;
	char *type;
	time_t chk_mtime = 0;
	struct uci_context *ctx;
	int uci_idx;
	/* update Value from uci */
	section = NULL;
	type = NULL;
	if (false) {
		section = NULL;
		type = NULL;
#if defined(AI)
	} else if (update_object_type == OBJECT_ANALOG_INPUT) {
		section = "bacnet_ai";
		type = "ai";
#endif
#if defined(AO)
	} else if (update_object_type == OBJECT_ANALOG_OUTPUT) {
		section = "bacnet_ao";
		type = "ao";
#endif
#if defined(AV)
	} else if (update_object_type == OBJECT_ANALOG_VALUE) {
		section = "bacnet_av";
		type = "av";
#endif
#if defined(BI)
	} else if (update_object_type == OBJECT_BINARY_INPUT) {
		section = "bacnet_bi";
		type = "bi";
#endif
#if defined(BO)
	} else if (update_object_type == OBJECT_BINARY_OUTPUT) {
		section = "bacnet_bo";
		type = "bo";
#endif
#if defined(BV)
	} else if (update_object_type == OBJECT_BINARY_VALUE) {
		section = "bacnet_bv";
		type = "bv";
#endif
#if defined(MI)
	} else if (update_object_type == OBJECT_MULTI_STATE_INPUT) {
		section = "bacnet_mi";
		type = "mi";
#endif
#if defined(MO)
	} else if (update_object_type == OBJECT_MULTI_STATE_OUTPUT) {
		section = "bacnet_mo";
		type = "mo";
#endif
#if defined(MSV)
	} else if (update_object_type == OBJECT_MULTI_STATE_VALUE) {
		section = "bacnet_mv";
		type = "mv";
#endif
	} else {
		return 0;
	}
	if ( ucirewrite == 0) {
		chk_mtime = ucimodtime;
#if PRINT_ENABLED
		printf("rewrite type: %s\n", type);
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
			uci_idx = atoi(cur->idx);
#if PRINT_ENABLED
			printf("section %s idx %i \n", section, uci_idx);
#endif
			if (false) {
			}
/* update Analog Input from uci */
#if defined(AI)
			else if (update_object_type == OBJECT_ANALOG_INPUT) {
				float ai_val, ai_pval;
				ai_val = strtof(cur->value,NULL);
				ai_pval = Analog_Input_Present_Value(uci_idx);
				if ( ai_val != ai_pval ) {
					Analog_Input_Present_Value_Set(uci_idx,ai_val,16);
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
			}
#endif
/* update Analog Output from uci */
#if defined(AO)
			else if (update_object_type == OBJECT_ANALOG_OUTPUT) {
				float ao_val, ao_pval;
				ao_val = strtof(cur->value,NULL);
				ao_pval = Analog_Output_Present_Value(uci_idx);
				if ( ao_val != ao_pval ) {
					Analog_Output_Present_Value_Set(uci_idx,ao_val,16);
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
			}
#endif
/* update Analog Value from uci */
#if defined(AV)
			else if (update_object_type == OBJECT_ANALOG_VALUE) {
				float av_val, av_pval;
				av_val = strtof(cur->value,NULL);
				av_pval = Analog_Value_Present_Value(uci_idx);
				if ( av_val != av_pval ) {
					Analog_Value_Present_Value_Set(uci_idx,av_val,16);
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
			}
#endif
/* update Binary Input from uci */
#if defined(BI)
			else if (update_object_type == OBJECT_BINARY_INPUT) {
				int bi_val, bi_pval;
				bi_val = atoi(cur->value);
				bi_pval = Binary_Input_Present_Value(uci_idx);
				if ( bi_val != bi_pval ) {
					Binary_Input_Present_Value_Set(uci_idx,bi_val,16);
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
			}
#endif
/* update Binary Output from uci */
#if defined(BO)
			else if (update_object_type == OBJECT_BINARY_OUTPUT) {
				int bo_val, bo_pval;
				bo_val = atoi(cur->value);
				bo_pval = Binary_Output_Present_Value(uci_idx);
				if ( bo_val != bo_pval ) {
					Binary_Output_Present_Value_Set(uci_idx,bo_val,16);
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
			}
#endif
/* update Binary Value from uci */
#if defined(BV)
			else if (update_object_type == OBJECT_BINARY_VALUE) {
				int bv_val, bv_pval;
				bv_val = atoi(cur->value);
				bv_pval = Binary_Value_Present_Value(uci_idx);
				if ( bv_val != bv_pval ) {
					Binary_Value_Present_Value_Set(uci_idx,bv_val,16);
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
			}
#endif
/* update Multistate Input from uci */
#if defined(MSI)
			else if (update_object_type == OBJECT_MULTI_STATE_INPUT) {
				int msi_val, msi_pval;
				msi_val = atoi(cur->value);
				msi_pval = Multistate_Input_Present_Value(uci_idx);
				if ( msi_val != msi_pval ) {
					Multistate_Input_Present_Value_Set(uci_idx,msi_val,16);
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
				}
			}
#endif
/* update Multistate Output from uci */
#if defined(MSO)
			else if (update_object_type == OBJECT_MULTI_STATE_OUTPUT) {
				int mso_val, mso_pval;
				mso_val = atoi(cur->value);
				mso_pval = Multistate_Output_Present_Value(uci_idx);
				if ( mso_val != mso_pval ) {
					Multistate_Output_Present_Value_Set(uci_idx,mso_val,16);
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
			}
#endif
/* update Multistate Value from uci */
#if defined(MSV)
			else if (update_object_type == OBJECT_MULTI_STATE_VALUE) {
				int msv_val, msv_pval;
				msv_val = atoi(cur->value);
				msv_pval = Multistate_Value_Present_Value(uci_idx);
				if ( msv_val != msv_pval ) {
					Multistate_Value_Present_Value_Set(uci_idx,msv_val,16);
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
#endif
		}
		ucix_cleanup(ctx);
	}
	/* update end */
	return ucimodtime;
}
#endif
#endif

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

#if 0
	/* 	BACnet Testing Observed Incident oi00107
		Server only devices should not indicate that they EXECUTE I-Am
		Revealed by BACnet Test Client v1.8.16 ( www.bac-test.com/bacnet-test-client-download )
			BITS: BIT00040
		Any discussions can be directed to edward@bac-test.com
		Please feel free to remove this comment when my changes accepted after suitable time for
		review by all interested parties. Say 6 months -> September 2016 */
	/* In this demo, we are the server only ( BACnet "B" device ) so we do not indicate
	   that we can execute the I-Am message */
    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_bind);
#endif

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
#if defined(BACNET_TIME_MASTER)
    handler_timesync_init();
#endif
}

static void print_usage(const char *filename)
{
    printf("Usage: %s [device-instance [device-name]]\n", filename);
    printf("       [--version][--help]\n");
}

static void print_help(const char *filename)
{
    printf("Simulate a BACnet server device\n"
        "device-instance:\n"
        "BACnet Device Object Instance number that you are\n"
        "trying simulate.\n"
        "device-name:\n"
        "The Device object-name is the text name for the device.\n"
        "\nExample:\n");
    printf("To simulate Device 123, use the following command:\n"
        "%s 123\n", filename);
    printf("To simulate Device 123 named Fred, use following command:\n"
        "%s 123 Fred\n", filename);
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
#if defined(INTRINSIC_REPORTING)
    uint32_t recipient_scan_tmr = 0;
#endif
#if defined(BACNET_TIME_MASTER)
    BACNET_DATE_TIME bdatetime;
#endif
#if defined(BAC_UCI)
    int uciId = 0;
    const char *uciName;
    struct uci_context *ctx;
#if defined(AI)
    time_t ucimodtime_bacnet_ai = 0;
#endif
#if defined(AO)
    time_t ucimodtime_bacnet_ao = 0;
#endif
#if defined(AV)
    time_t ucimodtime_bacnet_av = 0;
#endif
#if defined(BI)
    time_t ucimodtime_bacnet_bi = 0;
#endif
#if defined(BO)
    time_t ucimodtime_bacnet_bo = 0;
#endif
#if defined(BV)
    time_t ucimodtime_bacnet_bv = 0;
#endif
#if defined(MSI)
    time_t ucimodtime_bacnet_mi = 0;
#endif
#if defined(MSO)
    time_t ucimodtime_bacnet_mo = 0;
#endif
#if defined(MSV)
    time_t ucimodtime_bacnet_mv = 0;
#endif
    int rewrite = 0;
#endif
    int argi = 0;
    const char *filename = NULL;

    filename = filename_remove_path(argv[0]);
    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--help") == 0) {
            print_usage(filename);
            print_help(filename);
            return 0;
        }
        if (strcmp(argv[argi], "--version") == 0) {
            printf("%s %s\n", filename, BACNET_VERSION_TEXT);
            printf("Copyright (C) 2014 by Steve Karg and others.\n"
                "This is free software; see the source for copying conditions.\n"
                "There is NO warranty; not even for MERCHANTABILITY or\n"
                "FITNESS FOR A PARTICULAR PURPOSE.\n");
            return 0;
        }
    }
#if defined(BAC_UCI)
    char *pEnv = getenv("UCI_SECTION");
    if (!pEnv) {
       	pEnv = "0";
#if PRINT_ENABLED
        fprintf(stderr,  "Failed to getenv(UCI_SECTION)\n");
    } else {
	    fprintf(stderr,  "load config file bacnet_dev %s\n",pEnv);
#endif
    }

    ctx = ucix_init("bacnet_dev");
#if PRINT_ENABLED
    if(!ctx)
        fprintf(stderr,  "Failed to load config file bacnet_dev\n");
#endif

    uciId = ucix_get_option_int(ctx, "bacnet_dev", pEnv, "Id", 0);
    uciName = ucix_get_option(ctx, "bacnet_dev", pEnv, "name");
    if(ctx) 
        ucix_cleanup(ctx);
    if (uciId != 0) {
        Device_Set_Object_Instance_Number(uciId);
        if (uciName)
#if PRINT_ENABLED
            fprintf(stderr, "BACnet Device Name: %s\n", uciName);
#endif
            Device_Object_Name_ANSI_Init(uciName);
    } else {
#endif /* defined(BAC_UCI) */
        /* allow the device ID to be set */
        if (argc > 1) {
            Device_Set_Object_Instance_Number(strtol(argv[1], NULL, 0));
        }
        if (argc > 2) {
            Device_Object_Name_ANSI_Init(argv[2]);
        }
#if defined(BAC_UCI)
    }

#if defined(AI)
    char ai_path[128];
    struct stat ai_s;
	snprintf(ai_path, sizeof(ai_path), "/etc/config/bacnet_ai");
	if( stat(ai_path, &ai_s) > -1 )
		ucimodtime_bacnet_ai = ai_s.st_mtime;
#endif
#if defined(AO)
    char ao_path[128];
    struct stat ao_s;
	snprintf(ao_path, sizeof(ao_path), "/etc/config/bacnet_ao");
	if( stat(ao_path, &ao_s) > -1 )
		ucimodtime_bacnet_ao = ao_s.st_mtime;
#endif
#if defined(AV)
    char av_path[128];
    struct stat av_s;
	snprintf(av_path, sizeof(av_path), "/etc/config/bacnet_av");
	if( stat(av_path, &av_s) > -1 )
		ucimodtime_bacnet_av = av_s.st_mtime;
#endif
#if defined(BI)
    char bi_path[128];
    struct stat bi_s;
	snprintf(bi_path, sizeof(bi_path), "/etc/config/bacnet_bi");
	if( stat(bi_path, &bi_s) > -1 )
		ucimodtime_bacnet_bi = bi_s.st_mtime;
#endif
#if defined(BO)
    char bo_path[128];
    struct stat bo_s;
	snprintf(bo_path, sizeof(bo_path), "/etc/config/bacnet_bo");
	if( stat(bo_path, &bo_s) > -1 )
		ucimodtime_bacnet_bo = bo_s.st_mtime;
#endif
#if defined(BV)
    char bv_path[128];
    struct stat bv_s;
	snprintf(bv_path, sizeof(bv_path), "/etc/config/bacnet_bv");
	if( stat(bv_path, &bv_s) > -1 )
		ucimodtime_bacnet_bv = bv_s.st_mtime;
#endif
#if defined(MSI)
    char msi_path[128];
    struct stat msi_s;
	snprintf(msi_path, sizeof(msi_path), "/etc/config/bacnet_mi");
	if( stat(msi_path, &msi_s) > -1 )
		ucimodtime_bacnet_mi = msi_s.st_mtime;
#endif
#if defined(MSO)
    char mso_path[128];
    struct stat mso_s;
	snprintf(mso_path, sizeof(mso_path), "/etc/config/bacnet_mo");
	if( stat(mso_path, &mso_s) > -1 )
		ucimodtime_bacnet_mo = mso_s.st_mtime;
#endif
#if defined(MSV)
    char msv_path[128];
    struct stat msv_s;
	snprintf(msv_path, sizeof(msv_path), "/etc/config/bacnet_mv");
	if( stat(msv_path, &msv_s) > -1 )
		ucimodtime_bacnet_mv = msv_s.st_mtime;
#endif
#endif /* defined(BAC_UCI) */

#if PRINT_ENABLED
    printf("BACnet Server Demo\n" "BACnet Stack Version %s\n"
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
#if defined(LC)
            Load_Control_State_Machine_Handler();
#endif
            elapsed_milliseconds = elapsed_seconds * 1000;
            handler_cov_timer_seconds(elapsed_seconds);
            tsm_timer_milliseconds(elapsed_milliseconds);
#if defined(TRENDLOG)
            trend_log_timer(elapsed_seconds);
#endif
#if defined(INTRINSIC_REPORTING)
            Device_local_reporting();
#endif
#if defined(BACNET_TIME_MASTER)
            Device_getCurrentDateTime(&bdatetime);
            handler_timesync_task(&bdatetime);
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
        /* output */
#if defined(BAC_UCI)
        rewrite++;
        if (rewrite>100000) {
#if PRINT_ENABLED
            printf("rewrite interval %i\n", rewrite);
#endif
            rewrite=0;
        }
#if defined(AI)
        /* update Analog Input from uci */
        ucimodtime_bacnet_ai = uci_Update(ucimodtime_bacnet_ai,OBJECT_ANALOG_INPUT,rewrite);
#endif
#if defined(AO)
        /* update Analog Output from uci */
        ucimodtime_bacnet_ao = uci_Update(ucimodtime_bacnet_ao,OBJECT_ANALOG_OUTPUT,rewrite);
#endif
#if defined(AV)
        /* update Analog Value from uci */
        ucimodtime_bacnet_av = uci_Update(ucimodtime_bacnet_av,OBJECT_ANALOG_VALUE,rewrite);
#endif
#if defined(BI)
        /* update Binary Input from uci */
        ucimodtime_bacnet_bi = uci_Update(ucimodtime_bacnet_bi,OBJECT_BINARY_INPUT,rewrite);
#endif
#if defined(BO)
        /* update Binary Output from uci */
        ucimodtime_bacnet_bo = uci_Update(ucimodtime_bacnet_bo,OBJECT_BINARY_OUTPUT,rewrite);
#endif
#if defined(BV)
        /* update Binary Value from uci */
        ucimodtime_bacnet_bv = uci_Update(ucimodtime_bacnet_bv,OBJECT_BINARY_VALUE,rewrite);
#endif
#if defined(MSI)
        /* update Multistate Input from uci */
        ucimodtime_bacnet_mi = uci_Update(ucimodtime_bacnet_mi,OBJECT_MULTI_STATE_INPUT,rewrite);
#endif
#if defined(MSO)
        /* update Multistate Output from uci */
        ucimodtime_bacnet_mo = uci_Update(ucimodtime_bacnet_mo,OBJECT_MULTI_STATE_OUTPUT,rewrite);
#endif
#if defined(MSV)
        /* update Multistate Value from uci */
        ucimodtime_bacnet_mv = uci_Update(ucimodtime_bacnet_mv,OBJECT_MULTI_STATE_VALUE,rewrite);
#endif
#endif /* defined(BAC_UCI) */

        /* blink LEDs, Turn on or off outputs, etc */
    }

    return 0;
}

/* @} */

/* End group ServerDemo */
