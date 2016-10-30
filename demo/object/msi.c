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

/* Multi-state Value Objects */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bacdef.h"
#include "bacdcode.h"
#include "bacenum.h"
#include "bacapp.h"
#include "bactext.h"
#include "config.h"     /* the custom stuff */
#include "device.h"
#include "handlers.h"
#include "msi.h"
#include "ucix.h"

/* number of demo objects */
#ifndef MAX_MULTI_STATE_INPUTS
#define MAX_MULTI_STATE_INPUTS 1024
#endif
unsigned max_multi_state_inputs_int = 0;

/* When all the priorities are level null, the present value returns */
/* the Relinquish Default value */
#define MULTI_STATE_INPUT_RELINQUISH_DEFAULT 0

/* we choose to have a NULL level in our system represented by */
/* a particular value.  When the priorities are not in use, they */
/* will be relinquished (i.e. set to the NULL level). */
#define MULTI_STATE_LEVEL_NULL 255

MULTI_STATE_INPUT_DESCR MSI_Descr[MAX_MULTI_STATE_INPUTS];

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Multistate_Input_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,
    PROP_STATUS_FLAGS,
    PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,
    PROP_NUMBER_OF_STATES,
    -1
};

static const int Multistate_Input_Properties_Optional[] = {
    PROP_DESCRIPTION,
    PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
    PROP_STATE_TEXT,
#if defined(INTRINSIC_REPORTING)
    PROP_ALARM_VALUES,
    PROP_TIME_DELAY,
    PROP_NOTIFICATION_CLASS,
    PROP_EVENT_ENABLE,
    PROP_ACKED_TRANSITIONS,
    PROP_NOTIFY_TYPE,
    PROP_EVENT_TIME_STAMPS,
#endif
    PROP_RELIABILITY,
    -1
};

static const int Multistate_Input_Properties_Proprietary[] = {
    -1
};

struct uci_context *ctx;

void Multistate_Input_Property_Lists(
    const int **pRequired,
    const int **pOptional,
    const int **pProprietary)
{
    if (pRequired)
        *pRequired = Multistate_Input_Properties_Required;
    if (pOptional)
        *pOptional = Multistate_Input_Properties_Optional;
    if (pProprietary)
        *pProprietary = Multistate_Input_Properties_Proprietary;

    return;
}

void Multistate_Input_Load_UCI_List(const char *sec_idx,
	struct mi_inst_itr_ctx *itr)
{
	mi_inst_tuple_t *t = malloc(sizeof(mi_inst_tuple_t));
	bool disable;
	disable = ucix_get_option_int(itr->ctx, itr->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx,"default") == 0)
		return;
	if (disable)
		return;
	if( (t = (mi_inst_tuple_t *)malloc(sizeof(mi_inst_tuple_t))) != NULL ) {
		strncpy(t->idx, sec_idx, sizeof(t->idx));
		t->next = itr->list;
		itr->list = t;
	}
    return;
}


/*
 * Things to do when starting up the stack for Multistate Value.
 * Should be called whenever we reset the device or power it up
 */
void Multistate_Input_Init(
    void)
{
    unsigned i, j;
    static bool initialized = false;
    char name[64];
    const char *uciname;
    int ucidisable;
    uint8_t ucivalue;
    char description[64];
    const char *ucidescription;
    const char *ucidescription_default;
    const char *idx_c;
    char idx_cc[64];
    char *ucistate_default[254];
    int ucistate_n_default = 0;
#if defined(INTRINSIC_REPORTING)
    char *ucialarmstate_default[254];
    int ucialarmstate_n_default = 0;
    int ucinc_default;
    int ucinc;
    int ucievent_default;
    int ucievent;
    int ucitime_delay_default;
    int ucitime_delay;
#endif
    const char *sec = "bacnet_mi";

	char *section;
	char *type;
	struct mi_inst_itr_ctx itr_m;
	section = "bacnet_mi";

#if PRINT_ENABLED
    fprintf(stderr, "Multistate_Input_Init\n");
#endif
    if (!initialized) {
        initialized = true;
        ctx = ucix_init(sec);
#if PRINT_ENABLED
        if(!ctx)
            fprintf(stderr,  "Failed to load config file bacnet_mi\n");
#endif
		type = "mi";
		mi_inst_tuple_t *cur = malloc(sizeof (mi_inst_tuple_t));
		itr_m.list = NULL;
		itr_m.section = section;
		itr_m.ctx = ctx;
		ucix_for_each_section_type(ctx, section, type,
			(void *)Multistate_Input_Load_UCI_List, &itr_m);

        ucidescription_default = ucix_get_option(ctx, "bacnet_mi", "default",
            "description");
        ucistate_n_default = ucix_get_list(ucistate_default, ctx,
            "bacnet_mi", "default", "state");
#if defined(INTRINSIC_REPORTING)
        ucialarmstate_n_default = ucix_get_list(ucialarmstate_default, ctx,
            "bacnet_mi", "default", "alarmstate");
        ucinc_default = ucix_get_option_int(ctx, "bacnet_mi", "default",
            "nc", -1);
        ucievent_default = ucix_get_option_int(ctx, "bacnet_mi", "default",
            "event", -1);
        ucitime_delay_default = ucix_get_option_int(ctx, "bacnet_mi", "default",
            "time_delay", -1);
#endif
        i = 0;
		for( cur = itr_m.list; cur; cur = cur->next ) {
			strncpy(idx_cc, cur->idx, sizeof(idx_cc));
            idx_c = idx_cc;
            char *ucistate[254];
            int ucistate_n = 0;
            char *state[254];
            int state_n = 0;
#if defined(INTRINSIC_REPORTING)
            char *ucialarmstate[254];
            int ucialarmstate_n = 0;
            char *alarmstate[254];
            int alarmstate_n = 0;
#endif
            uciname = ucix_get_option(ctx, "bacnet_mi", idx_c, "name");
            ucidisable = ucix_get_option_int(ctx, "bacnet_mi", idx_c,
                "disable", 0);
            if ((uciname != 0) && (ucidisable == 0)) {
                memset(&MSI_Descr[i], 0x00, sizeof(MULTI_STATE_INPUT_DESCR));
                /* initialize all the analog output priority arrays to NULL */
                for (j = 0; j < BACNET_MAX_PRIORITY; j++) {
                    MSI_Descr[i].Priority_Array[j] = MULTI_STATE_LEVEL_NULL;
                }
                MSI_Descr[i].Instance=atoi(idx_cc);
                MSI_Descr[i].Disable=false;
                sprintf(name, "%s", uciname);
                ucix_string_copy(MSI_Descr[i].Object_Name,
                    sizeof(MSI_Descr[i].Object_Name), name);
                ucidescription = ucix_get_option(ctx, "bacnet_mi",
                    idx_c, "description");
                if (ucidescription != 0) {
                    sprintf(description, "%s", ucidescription);
                } else if (ucidescription_default != 0) {
                    sprintf(description, "%s %lu", ucidescription_default,
                        (unsigned long) i);
                } else {
                    sprintf(description, "MV%lu no uci section configured",
                        (unsigned long) i);
                }
                ucix_string_copy(MSI_Descr[i].Object_Description,
                    sizeof(MSI_Descr[i].Object_Description), description);
    
                ucivalue = ucix_get_option_int(ctx, "bacnet_mi", idx_c,
                    "value", 1);
                MSI_Descr[i].Priority_Array[15] = ucivalue;
                MSI_Descr[i].Relinquish_Default = 1; //TODO read uci

                ucistate_n = ucix_get_list(ucistate, ctx,
                    "bacnet_mi", idx_c, "state");
                if (ucistate_n == 0) {
                    state_n = ucistate_n_default;
                    for (j = 0; j < state_n; j++) {
                        state[j] = "";
                        state[j] = ucistate_default[j];
                    }
                } else {
                    state_n = ucistate_n;
                    for (j = 0; j < state_n; j++) {
                        state[j] = ucistate[j];
                    }
                }
                MSI_Descr[i].number_of_states = state_n;

                for (j = 0; j < state_n; j++) {
                    if (state[j]) {
                        sprintf(MSI_Descr[i].State_Text[j], "%s", state[j]);
                    } else {
                        sprintf(MSI_Descr[i].State_Text[j], "STATUS: %i", j);
                    }
                }
#if defined(INTRINSIC_REPORTING)
                int k,l = 0;
                ucialarmstate_n = ucix_get_list(ucialarmstate, ctx,
                    "bacnet_mi", idx_c, "alarmstate");
                if (ucialarmstate_n == 0) {
                    alarmstate_n = ucialarmstate_n_default;
                    for (j = 0; j < alarmstate_n; j++) {
                        alarmstate[j] = ucialarmstate_default[j];
                    }
                } else {
                    alarmstate_n = ucialarmstate_n;
                    for (j = 0; j < alarmstate_n; j++) {
                        alarmstate[j] = ucialarmstate[j];
                    }
                }
                for (j = 0; j < state_n; j++) {
                    for (k = 0; k < alarmstate_n; k++) {
                        if (strcmp(state[j],alarmstate[k]) == 0) {
                            MSI_Descr[i].Alarm_Values[l] = j+1;
                            l++;
                        }
                    }
                }
                MSI_Descr[i].number_of_alarmstates = l;
                ucinc = ucix_get_option_int(ctx, "bacnet_mi", idx_c,
                    "nc", ucinc_default);
                ucievent = ucix_get_option_int(ctx, "bacnet_mi", idx_c,
                    "event", ucievent_default);
                ucitime_delay = ucix_get_option_int(ctx, "bacnet_mi", idx_c,
                    "time_delay", ucitime_delay_default);
                MSI_Descr[i].Event_State = EVENT_STATE_NORMAL;
                /* notification class not connected */
                if (ucinc > -1) MSI_Descr[i].Notification_Class = ucinc;
                else MSI_Descr[i].Notification_Class = BACNET_MAX_INSTANCE;
                if (ucievent > -1) MSI_Descr[i].Event_Enable = ucievent;
                else MSI_Descr[i].Event_Enable = 0;
                if (ucitime_delay > -1) MSI_Descr[i].Time_Delay = ucitime_delay;
                else MSI_Descr[i].Time_Delay = 0;
                /* initialize Event time stamps using wildcards
                   and set Acked_transitions */
                for (j = 0; j < MAX_BACNET_EVENT_TRANSITION; j++) {
                    datetime_wildcard_set(&MSI_Descr[i].Event_Time_Stamps[j]);
                    MSI_Descr[i].Acked_Transitions[j].bIsAcked = true;
                }
        
                /* Set handler for GetEventInformation function */
                handler_get_event_information_set(OBJECT_MULTI_STATE_INPUT,
                    Multistate_Input_Event_Information);
                /* Set handler for AcknowledgeAlarm function */
                handler_alarm_ack_set(OBJECT_MULTI_STATE_INPUT,
                    Multistate_Input_Alarm_Ack);
                /* Set handler for GetAlarmSummary Service */
                handler_get_alarm_summary_set(OBJECT_MULTI_STATE_INPUT,
                    Multistate_Input_Alarm_Summary);

                MSI_Descr[i].Notify_Type = NOTIFY_ALARM;
#endif
                i++;
                max_multi_state_inputs_int = i;
            }
        }
#if PRINT_ENABLED
        fprintf(stderr, "max_multi_state_inputs: %i\n", max_multi_state_inputs_int);
#endif
        if(ctx)
            ucix_cleanup(ctx);
    }
    return;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Multistate_Input_Instance_To_Index(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    int index,i;
    index = max_multi_state_inputs_int;
    for (i = 0; i < index; i++) {
    	CurrentMSI = &MSI_Descr[i];
    	if (CurrentMSI->Instance == object_instance) {
    		return i;
    	}
    }
    return MAX_MULTI_STATE_INPUTS;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Multistate_Input_Index_To_Instance(
    unsigned index)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    uint32_t instance;
	CurrentMSI = &MSI_Descr[index];
	instance = CurrentMSI->Instance;
	return instance;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Multistate_Input_Count(
    void)
{
    return max_multi_state_inputs_int;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Multistate_Input_Valid_Instance(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    index = Multistate_Input_Instance_To_Index(object_instance);
    if (index == MAX_MULTI_STATE_INPUTS) {
#if PRINT_ENABLED
        fprintf(stderr, "Multistate_Input_Valid_Instance %i invalid\n",object_instance);
#endif
    	return false;
    }
    CurrentMSI = &MSI_Descr[index];
    if (CurrentMSI->Disable == false)
            return true;

    return false;
}

bool Multistate_Input_Change_Of_Value(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    bool status = false;
    unsigned index = 0;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        status = CurrentMSI->Change_Of_Value;
    }

    return status;
}

void Multistate_Input_Change_Of_Value_Clear(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        CurrentMSI->Change_Of_Value = false;
    }

    return;
}


/* returns true if value has changed */
bool Multistate_Input_Encode_Value_List(
    uint32_t object_instance,
    BACNET_PROPERTY_VALUE * value_list)
{
    bool status = false;

    if (value_list) {
        value_list->propertyIdentifier = PROP_PRESENT_VALUE;
        value_list->propertyArrayIndex = BACNET_ARRAY_ALL;
        value_list->value.context_specific = false;
        value_list->value.tag = BACNET_APPLICATION_TAG_ENUMERATED;
        value_list->value.type.Enumerated =
            Multistate_Input_Present_Value(object_instance);
        value_list->priority = BACNET_NO_PRIORITY;
        value_list = value_list->next;
    }
    if (value_list) {
        value_list->propertyIdentifier = PROP_STATUS_FLAGS;
        value_list->propertyArrayIndex = BACNET_ARRAY_ALL;
        value_list->value.context_specific = false;
        value_list->value.tag = BACNET_APPLICATION_TAG_BIT_STRING;
        bitstring_init(&value_list->value.type.Bit_String);
        bitstring_set_bit(&value_list->value.type.Bit_String,
            STATUS_FLAG_IN_ALARM, false);
        bitstring_set_bit(&value_list->value.type.Bit_String,
            STATUS_FLAG_FAULT, false);
        bitstring_set_bit(&value_list->value.type.Bit_String,
            STATUS_FLAG_OVERRIDDEN, false);
        if (Multistate_Input_Out_Of_Service(object_instance)) {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, true);
        } else {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, false);
        }
        value_list->priority = BACNET_NO_PRIORITY;
    }
    status =  Multistate_Input_Change_Of_Value(object_instance);

    return status;
}

uint32_t Multistate_Input_Present_Value(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    uint32_t value = MULTI_STATE_INPUT_RELINQUISH_DEFAULT;
    unsigned index = 0; /* offset from instance lookup */
    unsigned i = 0;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        /* When all the priorities are level null, the present value returns */
        /* the Relinquish Default value */
        value = CurrentMSI->Relinquish_Default;
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (CurrentMSI->Priority_Array[i] != MULTI_STATE_LEVEL_NULL) {
                value = CurrentMSI->Priority_Array[i];
                break;
            }
        }
    }

    return value;
}

bool Multistate_Input_Present_Value_Set(
    uint32_t object_instance,
    uint32_t value,
    uint8_t priority)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        if (priority && (priority <= BACNET_MAX_PRIORITY) &&
            (priority != 6 /* reserved */ ) && (value > 0) &&
            (value <= CurrentMSI->number_of_states)) {
            CurrentMSI->Priority_Array[priority - 1] = (uint8_t) value;
            /* Note: you could set the physical output here to the next
                highest priority, or to the relinquish default if no
                priorities are set.
                However, if Out of Service is TRUE, then don't set the
                physical output.  This comment may apply to the
                main loop (i.e. check out of service before changing output) */
            if (priority == 8) {
                CurrentMSI->Priority_Array[15] = (uint8_t) value;
            }
            status = true;
        }
    }
    return status;
}

bool Multistate_Input_Out_Of_Service(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    bool value = false;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        value = CurrentMSI->Out_Of_Service;
    }

    return value;
}

void Multistate_Input_Out_Of_Service_Set(
    uint32_t object_instance,
    bool value)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        CurrentMSI->Out_Of_Service = value;
    }
}

void Multistate_Input_Reliability_Set(
    uint32_t object_instance,
    uint8_t value)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        CurrentMSI->Reliability = value;
    }
}

uint8_t Multistate_Input_Reliability(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    uint8_t value = 0;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        value = CurrentMSI->Reliability;
    }

    return value;
}

static char *Multistate_Input_Description(
    uint32_t object_instance)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    char *pName = NULL; /* return value */

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        pName = CurrentMSI->Object_Description;
    }

    return pName;
}

bool Multistate_Input_Description_Set(
    uint32_t object_instance,
    char *new_name)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        status = true;
        if (new_name) {
            for (i = 0; i < sizeof(CurrentMSI->Object_Description); i++) {
                CurrentMSI->Object_Description[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentMSI->Object_Description); i++) {
                CurrentMSI->Object_Description[i] = 0;
            }
        }
    }

    return status;
}

static bool Multistate_Input_Description_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentMSI->Object_Description)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentMSI->Object_Description,
                    sizeof(CurrentMSI->Object_Description),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",CurrentMSI->Instance);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_mi", idx_c,
                            "description", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_mi\n");
#endif
                    }
                }
            } else {
                *error_class = ERROR_CLASS_PROPERTY;
                *error_code = ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED;
            }
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
        }
    }

    return status;
}

/* note: the object name must be unique within this device */
bool Multistate_Input_Object_Name(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING * object_name)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        status = characterstring_init_ansi(object_name, CurrentMSI->Object_Name);
    }

    return status;
}

/* note: the object name must be unique within this device */
bool Multistate_Input_Name_Set(
    uint32_t object_instance,
    char *new_name)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        status = true;
        /* FIXME: check to see if there is a matching name */
        if (new_name) {
            for (i = 0; i < sizeof(CurrentMSI->Object_Name); i++) {
                CurrentMSI->Object_Name[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentMSI->Object_Name); i++) {
                CurrentMSI->Object_Name[i] = 0;
            }
        }
    }

    return status;
}

static bool Multistate_Input_Object_Name_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentMSI->Object_Name)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentMSI->Object_Name,
                    sizeof(CurrentMSI->Object_Name),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",CurrentMSI->Instance);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_mi", idx_c,
                            "name", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_mi\n");
#endif
                    }
                }
            } else {
                *error_class = ERROR_CLASS_PROPERTY;
                *error_code = ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED;
            }
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
        }
    }

    return status;
}


static char *Multistate_Input_State_Text(
    uint32_t object_instance,
    uint32_t state_index)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    char *pName = NULL; /* return value */

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        if ((state_index > 0) && (state_index <= CurrentMSI->number_of_states)) {
            state_index--;
            pName = CurrentMSI->State_Text[state_index];
        }
    }

    return pName;
}

bool Multistate_Input_State_Text_Set(
    uint32_t object_instance,
    uint32_t state_index,
    char *new_name)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        if ((state_index > 0) && (state_index <= CurrentMSI->number_of_states)) {
            state_index--;
            status = true;
            if (new_name) {
                for (i = 0; i < sizeof(CurrentMSI->State_Text[state_index]); i++) {
                    CurrentMSI->State_Text[state_index][i] = new_name[i];
                    if (new_name[i] == 0) {
                        break;
                    }
                }
            } else {
                for (i = 0; i < sizeof(CurrentMSI->State_Text[state_index]); i++) {
                    CurrentMSI->State_Text[state_index][i] = 0;
                }
            }
        }
    }

    return status;;
}

static bool Multistate_Input_State_Text_Write(
    uint32_t object_instance,
    uint32_t state_index,
    BACNET_CHARACTER_STRING * char_string,
    BACNET_ERROR_CLASS * error_class,
    BACNET_ERROR_CODE * error_code)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];
#if defined(INTRINSIC_REPORTING)
    char ucialarmstate[254][64];
    int ucialarmstate_n = 0;
    int j, alarm_value;
#endif    
    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
        sprintf(idx_cc,"%d",CurrentMSI->Instance);
        idx_c = idx_cc;
        if ((state_index > 0) && (state_index <= CurrentMSI->number_of_states)) {
            state_index--;
            length = characterstring_length(char_string);
            if (length <= sizeof(CurrentMSI->State_Text[state_index])) {
                encoding = characterstring_encoding(char_string);
                if (encoding == CHARACTER_UTF8) {
                    status =
                        characterstring_ansi_copy(CurrentMSI->State_Text[state_index],
                    sizeof(CurrentMSI->State_Text[state_index]), char_string);
                    ucix_set_list(ctx, "bacnet_mi", idx_c, "state",
                        CurrentMSI->State_Text, CurrentMSI->number_of_states);
#if defined(INTRINSIC_REPORTING)
                    ucialarmstate_n = CurrentMSI->number_of_alarmstates;
                    for (j = 0; j < ucialarmstate_n; j++) {
                        alarm_value = CurrentMSI->Alarm_Values[j];
                        length = sizeof(CurrentMSI->State_Text[state_index]);
                        sprintf(ucialarmstate[j], "%s",
                            CurrentMSI->State_Text[alarm_value-1]);
                    }
                    ucix_set_list(ctx, "bacnet_mi", idx_c, "alarmstate",
                        ucialarmstate, ucialarmstate_n);
#endif
                    if (!status) {
                        *error_class = ERROR_CLASS_PROPERTY;
                        *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    }
                } else {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                *error_class = ERROR_CLASS_PROPERTY;
                *error_code = ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED;
            }
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
        }
    }

    return status;
}


/* return apdu len, or BACNET_STATUS_ERROR on error */
int Multistate_Input_Read_Property(
    BACNET_READ_PROPERTY_DATA * rpdata)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    int len = 0;
    int apdu_len = 0;   /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint32_t present_value = 0;
    unsigned index = 0;
    unsigned i = 0;
    unsigned idx = 0;
    bool state = false;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;

    if (Multistate_Input_Valid_Instance(rpdata->object_instance)) {
        index = Multistate_Input_Instance_To_Index(rpdata->object_instance);
        CurrentMSI = &MSI_Descr[index];
    } else
        return BACNET_STATUS_ERROR;
    if (CurrentMSI->Disable)
        return BACNET_STATUS_ERROR;

    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len =
                encode_application_object_id(&apdu[0],
                OBJECT_MULTI_STATE_INPUT, rpdata->object_instance);
            break;

        case PROP_OBJECT_NAME:
            Multistate_Input_Object_Name(rpdata->object_instance,
                &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Multistate_Input_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                OBJECT_MULTI_STATE_INPUT);
            break;

        case PROP_PRESENT_VALUE:
            present_value =
                Multistate_Input_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;

        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
#if defined(INTRINSIC_REPORTING)
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM,
                CurrentMSI->Event_State ? true : false);
#else
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
#endif
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            if (Multistate_Input_Out_Of_Service(rpdata->object_instance)) {
                bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE,
                    true);
            } else {
                bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE,
                    false);
            }
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentMSI->Event_State);
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;

        case PROP_OUT_OF_SERVICE:
            state = CurrentMSI->Out_Of_Service;
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;

        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(&apdu[0], 
                CurrentMSI->Reliability);
            break;


        case PROP_PRIORITY_ARRAY:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0) {
                apdu_len =
                    encode_application_unsigned(&apdu[0], BACNET_MAX_PRIORITY);
            /* if no index was specified, then try to encode the entire list */
            /* into one packet. */
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
                    /* FIXME: check if we have room before adding it to APDU */
                    if (CurrentMSI->Priority_Array[i] == MULTI_STATE_LEVEL_NULL) {
                        len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        present_value = CurrentMSI->Priority_Array[i];
                        len =
                            encode_application_unsigned(&apdu[apdu_len],
                            present_value);
                    }
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
                        apdu_len += len;
                    } else {
                        rpdata->error_class = ERROR_CLASS_SERVICES;
                        rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                        apdu_len = BACNET_STATUS_ERROR;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= BACNET_MAX_PRIORITY) {
                    if (CurrentMSI->Priority_Array[rpdata->array_index - 1]
                        == MULTI_STATE_LEVEL_NULL)
                        apdu_len = encode_application_null(&apdu[0]);
                    else {
                        present_value =
                            CurrentMSI->Priority_Array[rpdata->array_index - 1];
                        apdu_len =
                            encode_application_unsigned(&apdu[0],present_value);
                    }
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            present_value = CurrentMSI->Relinquish_Default;
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;

        case PROP_NUMBER_OF_STATES:
            apdu_len =
                encode_application_unsigned(&apdu[apdu_len],
                CurrentMSI->number_of_states);
            break;

        case PROP_STATE_TEXT:
            if (rpdata->array_index == 0) {
                /* Array element zero is the number of elements in the array */
                apdu_len =
                    encode_application_unsigned(&apdu[0],
                    CurrentMSI->number_of_states);
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                /* if no index was specified, then try to encode the entire list */
                /* into one packet. */
                for (idx = 0; idx < CurrentMSI->number_of_states-1; idx++) {
                    characterstring_init_ansi(&char_string,
                        CurrentMSI->State_Text[idx]);
                    /* FIXME: this might go beyond MAX_APDU length! */
                    len =
                        encode_application_character_string(&apdu[apdu_len],
                        &char_string);
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
                        apdu_len += len;
                    } else {
                        rpdata->error_class = ERROR_CLASS_SERVICES;
                        rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                        apdu_len = BACNET_STATUS_ERROR;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= CurrentMSI->number_of_states) {
                    characterstring_init_ansi(&char_string,
                        Multistate_Input_State_Text(rpdata->object_instance,
                        rpdata->array_index));
                    apdu_len =
                        encode_application_character_string(&apdu[0],
                        &char_string);
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;

#if defined(INTRINSIC_REPORTING)
        case PROP_ALARM_VALUES:
            for (i = 0; i < CurrentMSI->number_of_alarmstates; i++) {
                len =
                    encode_application_unsigned(&apdu[apdu_len],
                        CurrentMSI->Alarm_Values[i]);
                apdu_len += len;
            }
            break;

        case PROP_TIME_DELAY:
            apdu_len =
                encode_application_unsigned(&apdu[0], CurrentMSI->Time_Delay);
            break;

        case PROP_NOTIFICATION_CLASS:
            apdu_len =
                encode_application_unsigned(&apdu[0],
                CurrentMSI->Notification_Class);
            break;

        case PROP_EVENT_ENABLE:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                (CurrentMSI->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true :
                false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                (CurrentMSI->Event_Enable & EVENT_ENABLE_TO_FAULT) ? true :
                false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                (CurrentMSI->Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true :
                false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_ACKED_TRANSITIONS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                CurrentMSI->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                CurrentMSI->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                CurrentMSI->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_NOTIFY_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentMSI->Notify_Type ? NOTIFY_EVENT : NOTIFY_ALARM);
            break;

        case PROP_EVENT_TIME_STAMPS:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0)
                apdu_len =
                    encode_application_unsigned(&apdu[0],
                    MAX_BACNET_EVENT_TRANSITION);
            /* if no index was specified, then try to encode the entire list */
            /* into one packet. */
            else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                for (i = 0; i < MAX_BACNET_EVENT_TRANSITION; i++) {;
                    len =
                        encode_opening_tag(&apdu[apdu_len],
                        TIME_STAMP_DATETIME);
                    len +=
                        encode_application_date(&apdu[apdu_len + len],
                        &CurrentMSI->Event_Time_Stamps[i].date);
                    len +=
                        encode_application_time(&apdu[apdu_len + len],
                        &CurrentMSI->Event_Time_Stamps[i].time);
                    len +=
                        encode_closing_tag(&apdu[apdu_len + len],
                        TIME_STAMP_DATETIME);

                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU)
                        apdu_len += len;
                    else {
                        rpdata->error_class = ERROR_CLASS_SERVICES;
                        rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                        apdu_len = BACNET_STATUS_ERROR;
                        break;
                    }
                }
            } else if (rpdata->array_index <= MAX_BACNET_EVENT_TRANSITION) {
                apdu_len =
                    encode_opening_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
                apdu_len +=
                    encode_application_date(&apdu[apdu_len],
                    &CurrentMSI->Event_Time_Stamps[rpdata->array_index].date);
                apdu_len +=
                    encode_application_time(&apdu[apdu_len],
                    &CurrentMSI->Event_Time_Stamps[rpdata->array_index].time);
                apdu_len +=
                    encode_closing_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
            } else {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
            break;
#endif



      default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->object_property != PROP_EVENT_TIME_STAMPS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool Multistate_Input_Write_Property(
    BACNET_WRITE_PROPERTY_DATA * wp_data)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    bool status = false;        /* return value */
    unsigned index = 0;
    int object_type = 0;
    uint32_t object_instance = 0;
    unsigned int priority = 0;
    uint8_t level = MULTI_STATE_LEVEL_NULL;
    int len = 0;
    int element_len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    uint32_t max_states = 0;
    uint32_t array_index = 0;
    ctx = ucix_init("bacnet_mi");
#if defined(INTRINSIC_REPORTING)
    const char index_c[32] = "";
#endif
    const char *idx_c;
    char idx_cc[64];
    char cur_value[16];
    time_t cur_value_time;

    /* decode the some of the request */
    len =
        bacapp_decode_application_data(wp_data->application_data,
        wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }

    if (Multistate_Input_Valid_Instance(wp_data->object_instance)) {
        index = Multistate_Input_Instance_To_Index(wp_data->object_instance);
        CurrentMSI = &MSI_Descr[index];
        sprintf(idx_cc,"%d",CurrentMSI->Instance);
        idx_c = idx_cc;
    } else {
        return false;
    }

    switch (wp_data->object_property) {
        case PROP_OBJECT_NAME:
            if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                /* All the object names in a device must be unique */
                if (Device_Valid_Object_Name(&value.type.Character_String,
                    &object_type, &object_instance)) {
                    if ((object_type == wp_data->object_type) &&
                        (object_instance == wp_data->object_instance)) {
                        /* writing same name to same object */
                        status = true;
                    } else {
                        status = false;
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_DUPLICATE_NAME;
                    }
                } else {
                    status = Multistate_Input_Object_Name_Write(
                        wp_data->object_instance,
                        &value.type.Character_String,
                        &wp_data->error_class,
                        &wp_data->error_code);
                }
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;
        case PROP_DESCRIPTION:
            if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                status = Multistate_Input_Description_Write(
                    wp_data->object_instance,
                    &value.type.Character_String,
                    &wp_data->error_class,
                    &wp_data->error_code);
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;
        case PROP_PRESENT_VALUE:
            if (value.tag == BACNET_APPLICATION_TAG_UNSIGNED_INT) {
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                if (Multistate_Input_Present_Value_Set(wp_data->object_instance,
                        value.type.Unsigned_Int, wp_data->priority)) {
                    status = true;
                    sprintf(cur_value,"%d",value.type.Unsigned_Int);
                    ucix_add_option(ctx, "bacnet_mi", idx_c, "value",
                        cur_value);
                    cur_value_time = time(NULL);
                    ucix_add_option_int(ctx, "bacnet_mi", idx_c, "value_time",
                        cur_value_time);
                    ucix_add_option_int(ctx, "bacnet_mi", idx_c, "write",
                        1);
                } else if (wp_data->priority == 6) {
                    /* Command priority 6 is reserved for use by Minimum On/Off
                       algorithm and may not be used for other purposes in any
                       object. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                status =
                    WPValidateArgType(&value, BACNET_APPLICATION_TAG_NULL,
                    &wp_data->error_class, &wp_data->error_code);
                if (status) {
                    level = MULTI_STATE_LEVEL_NULL;
                    priority = wp_data->priority;
                    if (priority && (priority <= BACNET_MAX_PRIORITY)) {
                        priority--;
                        CurrentMSI->Priority_Array[priority] = level;
                        /* Note: you could set the physical output here to the next
                           highest priority, or to the relinquish default if no
                           priorities are set.
                           However, if Out of Service is TRUE, then don't set the
                           physical output.  This comment may apply to the
                           main loop (i.e. check out of service before changing output) */
                    } else {
                        status = false;
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    }
                }
            }
            break;

        case PROP_OUT_OF_SERVICE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BOOLEAN,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Multistate_Input_Out_Of_Service_Set(wp_data->object_instance,
                    value.type.Boolean);
            }
            break;

        case PROP_RELIABILITY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentMSI->Reliability = value.type.Enumerated;
            }
            break;

        case PROP_STATE_TEXT:
            if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                if (wp_data->array_index == 0) {
                    /* Array element zero is the number of
                       elements in the array.  We have a fixed
                       size array, so we are read-only. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                    max_states = max_multi_state_inputs_int;
                    array_index = 1;
                    element_len = len;
                    do {
                        if (element_len) {
                            status =
                                Multistate_Input_State_Text_Write(wp_data->
                                object_instance, array_index,
                                &value.type.Character_String,
                                &wp_data->error_class, &wp_data->error_code);
                        }
                        max_states--;
                        array_index++;
                        if (max_states) {
                            element_len =
                                bacapp_decode_application_data(&wp_data->
                                application_data[len],
                                wp_data->application_data_len - len, &value);
                            if (element_len < 0) {
                                wp_data->error_class = ERROR_CLASS_PROPERTY;
                                wp_data->error_code =
                                    ERROR_CODE_VALUE_OUT_OF_RANGE;
                                break;
                            }
                            len += element_len;
                        }
                    } while (max_states);
                } else {
                    max_states = max_multi_state_inputs_int;
                    if (wp_data->array_index <= max_states) {
                        status =
                            Multistate_Input_State_Text_Write(wp_data->
                            object_instance, wp_data->array_index,
                            &value.type.Character_String,
                            &wp_data->error_class, &wp_data->error_code);
                    } else {
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                    }
                }
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentMSI->Relinquish_Default = value.type.Unsigned_Int;
            }
            break;

#if defined(INTRINSIC_REPORTING)
        case PROP_ALARM_VALUES:
            if (value.tag == BACNET_APPLICATION_TAG_UNSIGNED_INT) {
                if (wp_data->array_index == 0) {
                    /* Array element zero is the number of
                       elements in the array.  We have a fixed
                       size array, so we are read-only. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                    int j, alarm_value;
                    int array_index = 0;
                    char ucialarmstate[254][64];
                    int ucialarmstate_n = 0;
                    for (j = 0; j < CurrentMSI->number_of_states; j++) {
                        array_index++;
                        if (wp_data->application_data[array_index] > 0) {
                            alarm_value = wp_data->application_data[array_index];
                            CurrentMSI->Alarm_Values[j] = alarm_value;
                            sprintf(ucialarmstate[j], "%s", 
                                CurrentMSI->State_Text[alarm_value-1]);
                            ucialarmstate_n++;
                        }
                        array_index++;        
                    }
                    CurrentMSI->number_of_alarmstates = ucialarmstate_n;
                    ucix_set_list(ctx, "bacnet_mi", idx_c, "alarmstate",
                        ucialarmstate, ucialarmstate_n);
                }
            }
        case PROP_TIME_DELAY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentMSI->Time_Delay = value.type.Unsigned_Int;
                CurrentMSI->Remaining_Time_Delay = CurrentMSI->Time_Delay;
                ucix_add_option_int(ctx, "bacnet_mi", index_c, "time_delay", value.type.Unsigned_Int);
            }
            break;

        case PROP_NOTIFICATION_CLASS:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentMSI->Notification_Class = value.type.Unsigned_Int;
                ucix_add_option_int(ctx, "bacnet_mi", index_c, "nc", value.type.Unsigned_Int);
            }
            break;

        case PROP_EVENT_ENABLE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 3) {
                    CurrentMSI->Event_Enable = value.type.Bit_String.value[0];
                    ucix_add_option_int(ctx, "bacnet_mi", index_c, "event", value.type.Bit_String.value[0]);
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    status = false;
                }
            }
            break;

        case PROP_NOTIFY_TYPE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                switch ((BACNET_NOTIFY_TYPE) value.type.Enumerated) {
                    case NOTIFY_EVENT:
                        CurrentMSI->Notify_Type = 1;
                        break;
                    case NOTIFY_ALARM:
                        CurrentMSI->Notify_Type = 0;
                        break;
                    default:
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                        status = false;
                        break;
                }
            }
            break;
#endif
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
        case PROP_NUMBER_OF_STATES:
        case PROP_PRIORITY_ARRAY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
#if defined(INTRINSIC_REPORTING)
        case PROP_ACKED_TRANSITIONS:
        case PROP_EVENT_TIME_STAMPS:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
#endif
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }
    ucix_commit(ctx, "bacnet_mi");
    ucix_cleanup(ctx);
    return status;
}

void Multistate_Input_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    BACNET_CHARACTER_STRING msgText;
    unsigned index;
    uint8_t FromState = 0;
    uint8_t ToState;
    uint8_t PresentVal = 0;
    bool SendNotify = false;
    bool tonormal = true;
    int i;


    if (Multistate_Input_Valid_Instance(object_instance)) {
        index = Multistate_Input_Instance_To_Index(object_instance);
        CurrentMSI = &MSI_Descr[index];
    } else
        return;

    if (CurrentMSI->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        CurrentMSI->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = CurrentMSI->Ack_notify_data.EventState;

#if PRINT_ENABLED
        fprintf(stderr, "Send Acknotification for (%s,%d).\n",
            bactext_object_type_name(OBJECT_MULTI_STATE_INPUT), object_instance);
#endif /* PRINT_ENABLED */

        characterstring_init_ansi(&msgText, "AckNotification");

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Multistate_Input_Present_Value(object_instance);
        FromState = CurrentMSI->Event_State;
        switch (CurrentMSI->Event_State) {
            case EVENT_STATE_NORMAL:
                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the High_Limit for a minimum
                   period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-OFFNORMAL flag must be set in the Event_Enable property. */
                for ( i = 0; i < CurrentMSI->number_of_states; i++) {
            	    if (CurrentMSI->Alarm_Values[i]) {
                        if ((PresentVal == CurrentMSI->Alarm_Values[i]) &&
                            ((CurrentMSI->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                            EVENT_ENABLE_TO_OFFNORMAL)) {
                                if (!CurrentMSI->Remaining_Time_Delay)
                                    CurrentMSI->Event_State = EVENT_STATE_FAULT;
                                else
                                    CurrentMSI->Remaining_Time_Delay--;
                                break;
            	        }
                    }
                }
                /* value of the object is still in the same event state */
                CurrentMSI->Remaining_Time_Delay = CurrentMSI->Time_Delay;
                break;

            case EVENT_STATE_FAULT:
                for ( i = 0; i < CurrentMSI->number_of_states; i++) {
            	    if (CurrentMSI->Alarm_Values[i]) {
                        if (PresentVal == CurrentMSI->Alarm_Values[i]) {
                               tonormal = false;
            	        }
                    }
                }
                if ((tonormal) &&
                    ((CurrentMSI->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                    EVENT_ENABLE_TO_NORMAL)) {
                    if (!CurrentMSI->Remaining_Time_Delay)
                        CurrentMSI->Event_State = EVENT_STATE_NORMAL;
                    else
                        CurrentMSI->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                CurrentMSI->Remaining_Time_Delay = CurrentMSI->Time_Delay;
                break;

            default:
                return; /* shouldn't happen */
        }       /* switch (FromState) */

        ToState = CurrentMSI->Event_State;

        if (FromState != ToState) {
            /* Event_State has changed.
               Need to fill only the basic parameters of this type of event.
               Other parameters will be filled in common function. */

            switch (ToState) {
                case EVENT_STATE_FAULT:
                    characterstring_init_ansi(&msgText, "Goes to EVENT_STATE_FAULT");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_FAULT) {
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from EVENT_STATE_FAULT");
                    }
                    break;

                default:
                    break;
            }   /* switch (ToState) */

#if PRINT_ENABLED
            fprintf(stderr, "Event_State for (%s,%d) goes from %s to %s.\n",
                bactext_object_type_name(OBJECT_MULTI_STATE_INPUT),
                object_instance,
                bactext_event_state_name(FromState),
                bactext_event_state_name(ToState));
#endif /* PRINT_ENABLED */

            /* Notify Type */
            event_data.notifyType = CurrentMSI->Notify_Type;

            /* Send EventNotification. */
            SendNotify = true;
        }
    }


    if (SendNotify) {
        /* Event Object Identifier */
        event_data.eventObjectIdentifier.type = OBJECT_MULTI_STATE_INPUT;
        event_data.eventObjectIdentifier.instance = object_instance;

        /* Time Stamp */
        event_data.timeStamp.tag = TIME_STAMP_DATETIME;
        Device_getCurrentDateTime(&event_data.timeStamp.value.dateTime);

        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* fill Event_Time_Stamps */
            switch (ToState) {
                case EVENT_STATE_FAULT:
                    CurrentMSI->Event_Time_Stamps[TRANSITION_TO_FAULT] =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentMSI->Event_Time_Stamps[TRANSITION_TO_NORMAL] =
                        event_data.timeStamp.value.dateTime;
                    break;
            }
        }

        /* Notification Class */
        event_data.notificationClass = CurrentMSI->Notification_Class;

        /* Event Type */
        event_data.eventType = EVENT_OUT_OF_RANGE;

        /* Message Text */
        event_data.messageText = &msgText;

        /* Notify Type */
        /* filled before */

        /* From State */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION)
            event_data.fromState = FromState;

        /* To State */
        event_data.toState = CurrentMSI->Event_State;

        /* Event Values */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* Value that exceeded a limit. */
            event_data.notificationParams.outOfRange.exceedingValue =
                PresentVal;
            /* Status_Flags of the referenced object. */
            bitstring_init(&event_data.notificationParams.
                outOfRange.statusFlags);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_IN_ALARM,
                CurrentMSI->Event_State ? true : false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_OUT_OF_SERVICE,
                CurrentMSI->Out_Of_Service);
        }

        /* add data from notification class */
        Notification_Class_common_reporting_function(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) &&
            (event_data.ackRequired == true)) {
            switch (event_data.toState) {
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                case EVENT_STATE_OFFNORMAL:
                    CurrentMSI->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked =
                        false;
                    CurrentMSI->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    CurrentMSI->
                        Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                        false;
                    CurrentMSI->
                        Acked_Transitions[TRANSITION_TO_FAULT].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentMSI->
                        Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
                        false;
                    CurrentMSI->
                        Acked_Transitions[TRANSITION_TO_NORMAL].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;
            }
        }
    }
#endif /* defined(INTRINSIC_REPORTING) */
}


#if defined(INTRINSIC_REPORTING)
int Multistate_Input_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    //MULTI_STATE_INPUT_DESCR *CurrentMSI;
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;


    /* check index */
    if (index < max_multi_state_inputs_int) {
        //CurrentMSI = &MSI_Descr[index];
        /* Event_State not equal to NORMAL */
        IsActiveEvent = (MSI_Descr[index].Event_State != EVENT_STATE_NORMAL);

        /* Acked_Transitions property, which has at least one of the bits
           (TO-OFFNORMAL, TO-FAULT, TONORMAL) set to FALSE. */
        IsNotAckedTransitions =
            (MSI_Descr[index].
            Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked ==
            false) | (MSI_Descr[index].
            Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
            false) | (MSI_Descr[index].
            Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked == false);
    } else
        return -1;      /* end of list  */

    if ((IsActiveEvent) || (IsNotAckedTransitions)) {
        /* Object Identifier */
        getevent_data->objectIdentifier.type = OBJECT_MULTI_STATE_INPUT;
        getevent_data->objectIdentifier.instance =
            Multistate_Input_Index_To_Instance(index);
        /* Event State */
        getevent_data->eventState = MSI_Descr[index].Event_State;
        /* Acknowledged Transitions */
        bitstring_init(&getevent_data->acknowledgedTransitions);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_OFFNORMAL,
            MSI_Descr[index].
            Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_FAULT,
            MSI_Descr[index].Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_NORMAL,
            MSI_Descr[index].Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);
        /* Event Time Stamps */
        for (i = 0; i < 3; i++) {
            getevent_data->eventTimeStamps[i].tag = TIME_STAMP_DATETIME;
            getevent_data->eventTimeStamps[i].value.dateTime =
                MSI_Descr[index].Event_Time_Stamps[i];
        }
        /* Notify Type */
        getevent_data->notifyType = MSI_Descr[index].Notify_Type;
        /* Event Enable */
        bitstring_init(&getevent_data->eventEnable);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_OFFNORMAL,
            (MSI_Descr[index].Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true :
            false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_FAULT,
            (MSI_Descr[index].Event_Enable & EVENT_ENABLE_TO_FAULT) ? true :
            false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_NORMAL,
            (MSI_Descr[index].Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true :
            false);
        /* Event Priorities */
        Notification_Class_Get_Priorities(MSI_Descr[index].Notification_Class,
            getevent_data->eventPriorities);

        return 1;       /* active event */
    } else
        return 0;       /* no active event at this index */
}

int Multistate_Input_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    unsigned index;

    if (Multistate_Input_Valid_Instance(alarmack_data->
        eventObjectIdentifier.instance)) {
        index = Multistate_Input_Instance_To_Index(alarmack_data->
        eventObjectIdentifier.instance);
        CurrentMSI = &MSI_Descr[index];
    } else {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }

    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
            if (CurrentMSI->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked == false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentMSI->Acked_Transitions
                        [TRANSITION_TO_OFFNORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentMSI->
                    Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked = true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_FAULT:
            if (CurrentMSI->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentMSI->Acked_Transitions
                        [TRANSITION_TO_NORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentMSI->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_NORMAL:
            if (CurrentMSI->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentMSI->Acked_Transitions
                        [TRANSITION_TO_FAULT].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentMSI->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        default:
            return -2;
    }

    /* Need to send AckNotification. */
    CurrentMSI->Ack_notify_data.bSendAckNotify = true;
    CurrentMSI->Ack_notify_data.EventState = alarmack_data->eventStateAcked;

    /* Return OK */
    return 1;
}

int Multistate_Input_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    MULTI_STATE_INPUT_DESCR *CurrentMSI;
    /* check index */
    if (index < max_multi_state_inputs_int) {
        CurrentMSI = &MSI_Descr[index];
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((CurrentMSI->Event_State != EVENT_STATE_NORMAL) &&
            (CurrentMSI->Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = OBJECT_MULTI_STATE_INPUT;
            getalarm_data->objectIdentifier.instance =
                Multistate_Input_Index_To_Instance(index);
            /* Alarm State */
            getalarm_data->alarmState = CurrentMSI->Event_State;
            /* Acknowledged Transitions */
            bitstring_init(&getalarm_data->acknowledgedTransitions);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_OFFNORMAL,
                CurrentMSI->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_FAULT,
                CurrentMSI->Acked_Transitions[TRANSITION_TO_FAULT].
                bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_NORMAL,
                CurrentMSI->Acked_Transitions[TRANSITION_TO_NORMAL].
                bIsAcked);

            return 1;   /* active alarm */
        } else
            return 0;   /* no active alarm at this index */
    } else
        return -1;      /* end of list  */
}
#endif /* defined(INTRINSIC_REPORTING) */



#ifdef TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"

bool WPValidateArgType(
    BACNET_APPLICATION_DATA_VALUE * pValue,
    uint8_t ucExpectedTag,
    BACNET_ERROR_CLASS * pErrorClass,
    BACNET_ERROR_CODE * pErrorCode)
{
    pValue = pValue;
    ucExpectedTag = ucExpectedTag;
    pErrorClass = pErrorClass;
    pErrorCode = pErrorCode;

    return false;
}

void testMultistateInput(
    Test * pTest)
{
    BACNET_READ_PROPERTY_DATA rpdata;
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;

    Multistate_Input_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_MULTI_STATE_INPUT;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Multistate_Input_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_MULTI_STATE_INPUT
int main(
    void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Multi-state Input", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testMultistateInput);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void) ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif
#endif /* TEST */
