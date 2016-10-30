/**************************************************************************
*
* Copyright (C) 2012 Steve Karg <skarg@users.sourceforge.net>
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
#ifndef MULTI_STATE_OUTPUT_H
#define MULTI_STATE_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "bacdef.h"
#include "cov.h"
#include "bacerror.h"
#include "rp.h"
#include "wp.h"
#if defined(INTRINSIC_REPORTING)
#include "nc.h"
#include "alarm_ack.h"
#include "getevent.h"
#include "get_alarm_sum.h"
#endif /* INTRINSIC_REPORTING */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int max_multi_state_outputs;

    typedef struct multistate_output_descr {
        uint32_t Instance;
        char Object_Name[64];
        char Object_Description[64];
        uint8_t Present_Value;
        unsigned Event_State:3;
        bool Out_Of_Service;
        bool Change_Of_Value;
        uint8_t Reliability;
        bool Disable;
        uint8_t Units;
        char State_Text[254][64];
        int number_of_states;
        /* Here is our Priority Array.  They are supposed to be Real, but */
        /* we don't have that kind of memory, so we will use a single byte */
        /* and load a Real for returning the value when asked. */
        uint8_t Priority_Array[BACNET_MAX_PRIORITY];
        unsigned Relinquish_Default;
#if defined(INTRINSIC_REPORTING)
        int number_of_alarmstates;
        uint32_t Time_Delay;
        uint32_t Notification_Class;
        uint8_t Alarm_Values[254];
        uint8_t Feedback_Value;
        unsigned Event_Enable:3;
        unsigned Notify_Type:1;
        ACKED_INFO Acked_Transitions[MAX_BACNET_EVENT_TRANSITION];
        BACNET_DATE_TIME Event_Time_Stamps[MAX_BACNET_EVENT_TRANSITION];
        /* time to generate event notification */
        uint32_t Remaining_Time_Delay;
        /* AckNotification informations */
        ACK_NOTIFICATION Ack_notify_data;
#endif /* INTRINSIC_REPORTING */
    } MULTI_STATE_OUTPUT_DESCR;


/* value/name tuples */
struct mo_inst_tuple {
	char idx[18];
	struct mo_inst_tuple *next;
};

typedef struct mo_inst_tuple mo_inst_tuple_t;

/* structure to hold tuple-list and uci context during iteration */
struct mo_inst_itr_ctx {
	struct mo_inst_tuple *list;
	struct uci_context *ctx;
	char *section;
};


	void Multistate_Output_Load_UCI_List(
		const char *sec_idx,
		struct mo_inst_itr_ctx *itr);

    void Multistate_Output_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);

    bool Multistate_Output_Valid_Instance(
        uint32_t object_instance);

    unsigned Multistate_Output_Count(
        void);

    uint32_t Multistate_Output_Index_To_Instance(
        unsigned index);

    unsigned Multistate_Output_Instance_To_Index(
        uint32_t object_instance);

    int Multistate_Output_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);

    bool Multistate_Output_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);

    /* optional API */
    bool Multistate_Output_Object_Instance_Add(
        uint32_t instance);

    bool Multistate_Output_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);

    bool Multistate_Output_Name_Set(
        uint32_t object_instance,
        char *new_name);

    bool Multistate_Output_Present_Value_Set(
        uint32_t object_instance,
        uint32_t value,
        uint8_t priority);

    uint32_t Multistate_Output_Present_Value(
        uint32_t object_instance);

    bool Multistate_Output_Out_Of_Service(
        uint32_t object_instance);

    void Multistate_Output_Out_Of_Service_Set(
        uint32_t object_instance,
        bool value);

    uint8_t Multistate_Output_Reliability(
        uint32_t object_instance);

    void Multistate_Output_Reliability_Set(
        uint32_t object_instance,
        uint8_t value);

    bool Multistate_Output_Encode_Value_List(
        uint32_t object_instance,
        BACNET_PROPERTY_VALUE * value_list);

    bool Multistate_Output_Change_Of_Value(
        uint32_t instance);

    void Multistate_Output_Change_Of_Value_Clear(
        uint32_t instance);

    bool Multistate_Output_Description_Set(
        uint32_t object_instance,
        char *text_string);

    bool Multistate_Output_State_Text_Set(
        uint32_t object_instance,
        uint32_t state_index,
        char *new_name);

    bool Multistate_Output_Max_States_Set(
        uint32_t instance,
        uint32_t max_states_requested);

    /* note: header of Intrinsic_Reporting function is required
       even when INTRINSIC_REPORTING is not defined */
    void Multistate_Output_Intrinsic_Reporting(
        uint32_t object_instance);

#if defined(INTRINSIC_REPORTING)
    int Multistate_Output_Event_Information(
        unsigned index,
        BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

    int Multistate_Output_Alarm_Ack(
        BACNET_ALARM_ACK_DATA * alarmack_data,
        BACNET_ERROR_CODE * error_code);

    int Multistate_Output_Alarm_Summary(
        unsigned index,
        BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif

    void Multistate_Output_Init(
        void);

#ifdef TEST
#include "ctest.h"
    void testMultistate_Output(
        Test * pTest);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
