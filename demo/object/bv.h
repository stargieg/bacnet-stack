/**************************************************************************
*
* Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
*
* Copyright (C) 2013 Patrick Grimm <patrick@lunatiki.de>
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
#ifndef BV_H
#define BV_H

#include <stdbool.h>
#include <stdint.h>
#include "bacdef.h"
#include "cov.h"
#include "bacerror.h"
#include "wp.h"
#include "rp.h"
#if defined(INTRINSIC_REPORTING)
#include "nc.h"
#include "alarm_ack.h"
#include "getevent.h"
#include "get_alarm_sum.h"
#endif /* INTRINSIC_REPORTING */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    typedef struct binary_value_descr {
        uint32_t Instance;
        char Object_Name[64];
        char Object_Description[64];
        unsigned Event_State:3;
        BACNET_BINARY_PV Present_Value;
        BACNET_POLARITY Polarity;
        bool Out_Of_Service;
        bool Change_Of_Value;
        uint8_t Reliability;
        bool Disable;
        BACNET_CHARACTER_STRING Inactive_Text;
        BACNET_CHARACTER_STRING Active_Text;
        /* Here is our Priority Array.*/
        BACNET_BINARY_PV Priority_Array[BACNET_MAX_PRIORITY];
        BACNET_BINARY_PV Relinquish_Default;
#if defined(INTRINSIC_REPORTING)
        uint32_t Time_Delay;
        uint32_t Notification_Class;
        BACNET_BINARY_PV Alarm_Value;
        unsigned Event_Enable:3;
        unsigned Notify_Type:1;
        ACKED_INFO Acked_Transitions[MAX_BACNET_EVENT_TRANSITION];
        BACNET_DATE_TIME Event_Time_Stamps[MAX_BACNET_EVENT_TRANSITION];
        /* time to generate event notification */
        uint32_t Remaining_Time_Delay;
        /* AckNotification informations */
        ACK_NOTIFICATION Ack_notify_data;
#endif /* INTRINSIC_REPORTING */
    } BINARY_VALUE_DESCR;

/* value/name tuples */
struct bv_inst_tuple {
	char idx[18];
	struct bv_inst_tuple *next;
};

typedef struct bv_inst_tuple bv_inst_tuple_t;

/* structure to hold tuple-list and uci context during iteration */
struct bv_inst_itr_ctx {
	struct bv_inst_tuple *list;
	struct uci_context *ctx;
	char *section;
};

	void Binary_Value_Load_UCI_List(
		const char *sec_idx,
		struct bv_inst_itr_ctx *itr);


    void Binary_Value_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);

    bool Binary_Value_Valid_Instance(
        uint32_t object_instance);

    unsigned Binary_Value_Count(
        void);

    uint32_t Binary_Value_Index_To_Instance(
        unsigned index);

    unsigned Binary_Value_Instance_To_Index(
        uint32_t instance);

    bool Binary_Value_Object_Instance_Add(
        uint32_t instance);

    bool Binary_Value_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);

    bool Binary_Value_Name_Set(
        uint32_t object_instance,
        char *new_name);

    char *Binary_Value_Description(
        uint32_t instance);
    bool Binary_Value_Description_Set(
        uint32_t instance,
        char *new_name);

    char *Binary_Value_Inactive_Text(
        uint32_t instance);

    bool Binary_Value_Inactive_Text_Set(
        uint32_t instance,
        char *new_name);

    char *Binary_Value_Active_Text(
        uint32_t instance);

    bool Binary_Value_Active_Text_Set(
        uint32_t instance,
        char *new_name);

    BACNET_POLARITY Binary_Value_Polarity(
        uint32_t object_instance);

    bool Binary_Value_Polarity_Set(
        uint32_t object_instance,
        BACNET_POLARITY polarity);

    bool Binary_Value_Out_Of_Service(
        uint32_t object_instance);

    void Binary_Value_Out_Of_Service_Set(
        uint32_t object_instance,
        bool value);

    uint8_t Binary_Value_Reliability(
        uint32_t object_instance);

    void Binary_Value_Reliability_Set(
        uint32_t object_instance,
        uint8_t value);

    bool Binary_Value_Encode_Value_List(
        uint32_t object_instance,
        BACNET_PROPERTY_VALUE * value_list);

    bool Binary_Value_Change_Of_Value(
        uint32_t instance);

    void Binary_Value_Change_Of_Value_Clear(
        uint32_t instance);

    int Binary_Value_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);

    bool Binary_Value_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);

    BACNET_BINARY_PV Binary_Value_Present_Value(
        uint32_t object_instance);

    bool Binary_Value_Present_Value_Set(
        uint32_t object_instance,
        BACNET_BINARY_PV value,
        uint8_t priority);

    unsigned Binary_Value_Present_Value_Priority(
        uint32_t object_instance);

    /* note: header of Intrinsic_Reporting function is required
       even when INTRINSIC_REPORTING is not defined */
    void Binary_Value_Intrinsic_Reporting(
        uint32_t object_instance);

#if defined(INTRINSIC_REPORTING)
    int Binary_Value_Event_Information(
        unsigned index,
        BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

    int Binary_Value_Alarm_Ack(
        BACNET_ALARM_ACK_DATA * alarmack_data,
        BACNET_ERROR_CODE * error_code);

    int Binary_Value_Alarm_Summary(
        unsigned index,
        BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif

    void Binary_Value_Init(
        void);

#ifdef TEST
#include "ctest.h"
    void testBinary_Value(
        Test * pTest);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
