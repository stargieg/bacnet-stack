/**************************************************************************
*
* Copyright (C) 2005 Steve Karg <skarg@users.sourceforge.net>
* Copyright (C) 2011 Krzysztof Malorny <malornykrzysztof@gmail.com>
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

/* Analog Output Objects - customize for your use */

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
#include "proplist.h"
#include "timestamp.h"
#include "ao.h"
#include "ucix.h"

/* number of demo objects */
#ifndef MAX_ANALOG_OUTPUTS
#define MAX_ANALOG_OUTPUTS 1024
#endif
unsigned max_analog_outputs_int = 0;

/* When all the priorities are level null, the present value returns */
/* the Relinquish Default value */
#define ANALOG_RELINQUISH_DEFAULT 0

/* we choose to have a NULL level in our system represented by */
/* a particular value.  When the priorities are not in use, they */
/* will be relinquished (i.e. set to the NULL level). */
#define ANALOG_LEVEL_NULL 255

ANALOG_OUTPUT_DESCR AO_Descr[MAX_ANALOG_OUTPUTS];

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Analog_Output_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,
    PROP_STATUS_FLAGS,
    PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,
    PROP_UNITS,
    -1
};

static const int Analog_Output_Properties_Optional[] = {
    PROP_DESCRIPTION,
    PROP_RELIABILITY,
    PROP_COV_INCREMENT,
    PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
    PROP_MAX_PRES_VALUE,
    PROP_MIN_PRES_VALUE,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY,
    PROP_NOTIFICATION_CLASS,
    PROP_HIGH_LIMIT,
    PROP_LOW_LIMIT,
    PROP_DEADBAND,
    PROP_LIMIT_ENABLE,
    PROP_EVENT_ENABLE,
    PROP_ACKED_TRANSITIONS,
    PROP_NOTIFY_TYPE,
    PROP_EVENT_TIME_STAMPS,
#endif
    -1
};

static const int Analog_Output_Properties_Proprietary[] = {
    -1
};

struct uci_context *ctx;

void Analog_Output_Property_Lists(
    const int **pRequired,
    const int **pOptional,
    const int **pProprietary)
{
    if (pRequired)
        *pRequired = Analog_Output_Properties_Required;
    if (pOptional)
        *pOptional = Analog_Output_Properties_Optional;
    if (pProprietary)
        *pProprietary = Analog_Output_Properties_Proprietary;

    return;
}

void Analog_Output_Load_UCI_List(const char *sec_idx,
	struct ao_inst_itr_ctx *itr)
{
	ao_inst_tuple_t *t = malloc(sizeof(ao_inst_tuple_t));
	bool disable;
	disable = ucix_get_option_int(itr->ctx, itr->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx,"default") == 0)
		return;
	if (disable)
		return;
	if( (t = (ao_inst_tuple_t *)malloc(sizeof(ao_inst_tuple_t))) != NULL ) {
		strncpy(t->idx, sec_idx, sizeof(t->idx));
		t->next = itr->list;
		itr->list = t;
	}
    return;
}


void Analog_Output_Init(
    void)
{
    unsigned i, j;
    static bool initialized = false;
    char name[64];
    const char *uciname;
    int ucidisable;
    const char *ucivalue;
    int Out_Of_Service;
    char description[64];
    const char *ucidescription;
    const char *ucidescription_default;
    const char *idx_c;
    char idx_cc[64];
    int uciunit = 0;
    int uciunit_default = 0;
    const char *ucivalue_default;
    char max_value[64];
    const char *ucimax_value_default;
    const char *ucimax_value;
    char min_value[64];
    const char *ucimin_value_default;
    const char *ucimin_value;
#if defined(INTRINSIC_REPORTING)
    int ucinc_default;
    int ucinc;
    int ucievent_default;
    int ucievent;
    int ucitime_delay_default;
    int ucitime_delay;
    int ucilimit_default;
    int ucilimit;
    char high_limit[64];
    const char *ucihigh_limit_default;
    const char *ucihigh_limit;
    char low_limit[64];
    const char *ucilow_limit_default;
    const char *ucilow_limit;
    char dead_limit[64];
    const char *ucidead_limit_default;
    const char *ucidead_limit;
#endif
    char cov_increment[64];
    const char *ucicov_increment;
    const char *ucicov_increment_default;
    const char *sec = "bacnet_ao";

	char *section;
	char *type;
	struct ao_inst_itr_ctx itr_m;
	section = "bacnet_ao";

#if PRINT_ENABLED
    fprintf(stderr, "Analog_Output_Init\n");
#endif
    if (!initialized) {
        initialized = true;
        ctx = ucix_init(sec);
#if PRINT_ENABLED
        if(!ctx)
            fprintf(stderr, "Failed to load config file bacnet_ao\n");
#endif
		type = "ao";
		ao_inst_tuple_t *cur = malloc(sizeof (ao_inst_tuple_t));
		itr_m.list = NULL;
		itr_m.section = section;
		itr_m.ctx = ctx;
		ucix_for_each_section_type(ctx, section, type,
			(void *)Analog_Output_Load_UCI_List, &itr_m);

        ucidescription_default = ucix_get_option(ctx, sec, "default",
            "description");
        uciunit_default = ucix_get_option_int(ctx, sec, "default",
            "si_unit", 0);
        ucivalue_default = ucix_get_option(ctx, sec, "default",
            "value");
#if defined(INTRINSIC_REPORTING)
        ucinc_default = ucix_get_option_int(ctx, sec, "default",
            "nc", -1);
        ucievent_default = ucix_get_option_int(ctx, sec, "default",
            "event", -1);
        ucitime_delay_default = ucix_get_option_int(ctx, sec, "default",
            "time_delay", -1);
        ucilimit_default = ucix_get_option_int(ctx, sec, "default",
            "limit", -1);
        ucihigh_limit_default = ucix_get_option(ctx, sec, "default",
            "high_limit");
        ucilow_limit_default = ucix_get_option(ctx, sec, "default",
            "low_limit");
        ucidead_limit_default = ucix_get_option(ctx, sec, "default",
            "dead_limit");
#endif
        ucicov_increment_default = ucix_get_option(ctx, sec, "default",
            "cov_increment");
        ucimax_value_default = ucix_get_option(ctx, sec, "default",
            "max_value");
        ucimin_value_default = ucix_get_option(ctx, sec, "default",
            "min_value");
        i = 0;
		for( cur = itr_m.list; cur; cur = cur->next ) {
			strncpy(idx_cc, cur->idx, sizeof(idx_cc));
            idx_c = idx_cc;
            uciname = ucix_get_option(ctx, "bacnet_ao", idx_c, "name");
            ucidisable = ucix_get_option_int(ctx, "bacnet_ao", idx_c,
                "disable", 0);
            if ((uciname != 0) && (ucidisable == 0)) {
                memset(&AO_Descr[i], 0x00, sizeof(ANALOG_OUTPUT_DESCR));
                /* initialize all the analog output priority arrays to NULL */
                for (j = 0; j < BACNET_MAX_PRIORITY; j++) {
                    AO_Descr[i].Priority_Array[j] = ANALOG_LEVEL_NULL;
                }
                AO_Descr[i].Instance=atoi(idx_cc);
                AO_Descr[i].Disable=false;
                sprintf(name, "%s", uciname);
                ucix_string_copy(AO_Descr[i].Object_Name,
                    sizeof(AO_Descr[i].Object_Name), name);
                ucidescription = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "description");
                if (ucidescription != 0) {
                    sprintf(description, "%s", ucidescription);
                } else if (ucidescription_default != 0) {
                    sprintf(description, "%s %lu", ucidescription_default,
                        (unsigned long) i);
                } else {
                    sprintf(description, "AO%lu no uci section configured",
                        (unsigned long) i);
                }
                ucix_string_copy(AO_Descr[i].Object_Description,
                    sizeof(AO_Descr[i].Object_Description), description);
                uciunit = ucix_get_option_int(ctx, "bacnet_ao", idx_c,
                    "si_unit", 0);
                if (uciunit != 0) {
                    AO_Descr[i].Units = uciunit;
                } else if (uciunit_default != 0) {
                    AO_Descr[i].Units = uciunit_default;
                } else {
                    AO_Descr[i].Units = UNITS_PERCENT;
                }
                Out_Of_Service = ucix_get_option_int(ctx, "bacnet_ao", idx_c,
                    "Out_Of_Service", 1);
                if (Out_Of_Service == 0) {
                    AO_Descr[i].Reliability = RELIABILITY_NO_FAULT_DETECTED;
                    AO_Descr[i].Out_Of_Service = 0;
                } else {
                    AO_Descr[i].Reliability = RELIABILITY_COMMUNICATION_FAILURE;
                    AO_Descr[i].Out_Of_Service = 1;
                }

                ucivalue = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "value");
                if (ucivalue == NULL) {
                    if (ucivalue_default == NULL) {
                        ucivalue = 0;
                    } else {
                        ucivalue = ucivalue_default;
                        AO_Descr[i].Reliability = 
                            RELIABILITY_COMMUNICATION_FAILURE;
                    }
                }
                AO_Descr[i].Priority_Array[15] = strtof(ucivalue,
                    (char **) NULL);

                AO_Descr[i].Relinquish_Default = 0; //TODO read uci

                ucicov_increment = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "cov_increment");
                if (ucicov_increment != 0) {
                    sprintf(cov_increment, "%s", ucicov_increment);
                } else {
                    if (ucicov_increment_default != 0) {
                        sprintf(cov_increment, "%s", ucicov_increment_default);
                    } else {
                        sprintf(cov_increment, "%s", "0");
                    }
                }
                AO_Descr[i].COV_Increment = strtof(cov_increment,
                    (char **) NULL);
                AO_Descr[i].Changed = false;

                ucimax_value = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "max_value");
                if (ucimax_value != 0) {
                    sprintf(max_value, "%s", ucimax_value);
                } else {
                    if (ucimax_value_default != 0) {
                        sprintf(max_value, "%s", ucimax_value_default);
                    } else {
                        sprintf(max_value, "%s", "0");
                    }
                }
                AO_Descr[i].Max_Pres_Value = strtof(max_value, (char **) NULL);

                ucimin_value = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "min_value");
                if (ucimin_value != 0) {
                    sprintf(min_value, "%s", ucimin_value);
                } else {
                    if (ucimin_value_default != 0) {
                        sprintf(min_value, "%s", ucimin_value_default);
                    } else {
                        sprintf(min_value, "%s", "0");
                    }
                }
                AO_Descr[i].Min_Pres_Value = strtof(min_value, (char **) NULL);

#if defined(INTRINSIC_REPORTING)
                ucinc = ucix_get_option_int(ctx, "bacnet_ao", idx_c,
                    "nc", ucinc_default);
                ucievent = ucix_get_option_int(ctx, "bacnet_ao", idx_c,
                    "event", ucievent_default);
                ucitime_delay = ucix_get_option_int(ctx, "bacnet_ao", idx_c,
                    "time_delay", ucitime_delay_default);
                ucilimit = ucix_get_option_int(ctx, "bacnet_ao", idx_c,
                    "limit", ucilimit_default);
                ucihigh_limit = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "high_limit");
                if (ucihigh_limit != 0) {
                    sprintf(high_limit, "%s", ucihigh_limit);
                } else {
                    if (ucihigh_limit_default != 0) {
                        sprintf(high_limit, "%s", ucihigh_limit_default);
                    } else {
                        sprintf(high_limit, "%s", "0");
                    }
                }
                ucilow_limit = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "low_limit");
                if (ucilow_limit != 0) {
                    sprintf(low_limit, "%s", ucilow_limit);
                } else {
                    if (ucilow_limit_default != 0) {
                        sprintf(low_limit, "%s", ucilow_limit_default);
                    } else {
                        sprintf(low_limit, "%s", "0");
                    }
                }
                ucidead_limit = ucix_get_option(ctx, "bacnet_ao", idx_c,
                    "dead_limit");
                if (ucidead_limit != 0) {
                    sprintf(dead_limit, "%s", ucidead_limit);
                } else {
                    if (ucidead_limit_default != 0) {
                        sprintf(dead_limit, "%s", ucidead_limit_default);
                    } else {
                        sprintf(dead_limit, "%s", "0");
                    }
                }
                AO_Descr[i].Event_State = EVENT_STATE_NORMAL;
                /* notification class not connected */
                if (ucinc > -1) AO_Descr[i].Notification_Class = ucinc;
                else AO_Descr[i].Notification_Class = 0;
                if (ucievent > -1) AO_Descr[i].Event_Enable = ucievent;
                else AO_Descr[i].Event_Enable = 0;
                if (ucitime_delay > -1) AO_Descr[i].Time_Delay = ucitime_delay;
                else AO_Descr[i].Time_Delay = 0;
                if (ucilimit > -1) AO_Descr[i].Limit_Enable = ucilimit;
                else AO_Descr[i].Limit_Enable = 0;
                AO_Descr[i].High_Limit = strtof(high_limit, (char **) NULL);
                AO_Descr[i].Low_Limit = strtof(low_limit, (char **) NULL);
                AO_Descr[i].Deadband = strtof(dead_limit, (char **) NULL);

                /* initialize Event time stamps using wildcards
                   and set Acked_transitions */
                for (j = 0; j < MAX_BACNET_EVENT_TRANSITION; j++) {
                    datetime_wildcard_set(&AO_Descr[i].Event_Time_Stamps[j]);
                    AO_Descr[i].Acked_Transitions[j].bIsAcked = true;
                }

                /* Set handler for GetEventInformation function */
                handler_get_event_information_set(OBJECT_ANALOG_OUTPUT,
                    Analog_Output_Event_Information);
                /* Set handler for AcknowledgeAlarm function */
                handler_alarm_ack_set(OBJECT_ANALOG_OUTPUT, Analog_Output_Alarm_Ack);
                /* Set handler for GetAlarmSummary Service */
                handler_get_alarm_summary_set(OBJECT_ANALOG_OUTPUT,
                    Analog_Output_Alarm_Summary);
#endif
                i++;
                max_analog_outputs_int = i;
            }
        }
#if PRINT_ENABLED
        fprintf(stderr, "max_analog_outputs %i\n", max_analog_outputs_int);
#endif
        if(ctx)
            ucix_cleanup(ctx);
    }
    return;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Analog_Output_Instance_To_Index(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    int index,i;
    //int instance;
    index = max_analog_outputs_int;
    for (i = 0; i < index; i++) {
    	CurrentAO = &AO_Descr[i];
    	//instance = CurrentAO->Instance;
    	if (CurrentAO->Instance == object_instance) {
    		return i;
    	}
    }
    return MAX_ANALOG_OUTPUTS;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Analog_Output_Index_To_Instance(
    unsigned index)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    uint32_t instance;
	CurrentAO = &AO_Descr[index];
	instance = CurrentAO->Instance;
	return instance;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Analog_Output_Count(
    void)
{
    return max_analog_outputs_int;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Analog_Output_Valid_Instance(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    index = Analog_Output_Instance_To_Index(object_instance);
    if (index == MAX_ANALOG_OUTPUTS) {
#if PRINT_ENABLED
        fprintf(stderr, "Analog_Output_Valid_Instance %i invalid\n",object_instance);
#endif
    	return false;
    }
    CurrentAO = &AO_Descr[index];
    if (CurrentAO->Disable == false)
            return true;

    return false;
}

static void Analog_Output_COV_Detect(uint32_t object_instance,
    float value)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned int index = 0;
    float prior_value = 0.0;
    float cov_increment = 0.0;
    float cov_delta = 0.0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        prior_value = CurrentAO->Prior_Value;
        cov_increment = CurrentAO->COV_Increment;
        if (prior_value > value) {
            cov_delta = prior_value - value;
        } else {
            cov_delta = value - prior_value;
        }
        if (cov_delta >= cov_increment) {
            CurrentAO->Changed = true;
            CurrentAO->Prior_Value = value;
        }
    }
}

bool Analog_Output_Change_Of_Value(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    bool changed = false;
    unsigned index = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        changed = CurrentAO->Change_Of_Value;
    }

    return changed;
}

void Analog_Output_Change_Of_Value_Clear(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        CurrentAO->Change_Of_Value = false;
    }
}

/* returns true if value has changed */
bool Analog_Output_Encode_Value_List(
    uint32_t object_instance,
    BACNET_PROPERTY_VALUE * value_list)
{
    bool status = false;

    if (value_list) {
        value_list->propertyIdentifier = PROP_PRESENT_VALUE;
        value_list->propertyArrayIndex = BACNET_ARRAY_ALL;
        value_list->value.context_specific = false;
        value_list->value.tag = BACNET_APPLICATION_TAG_REAL;
        value_list->value.type.Real =
            Analog_Output_Present_Value(object_instance);
        value_list->value.next = NULL;
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
        if (Analog_Output_Out_Of_Service(object_instance)) {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, true);
        } else {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, false);
        }
        value_list->value.next = NULL;
        value_list->priority = BACNET_NO_PRIORITY;
        value_list->next = NULL;
    }
    status = Analog_Output_Change_Of_Value(object_instance);

    return status;
}

float Analog_Output_COV_Increment(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0;
    float value = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        value = CurrentAO->COV_Increment;
    }

    return value;
}

void Analog_Output_COV_Increment_Set(
    uint32_t object_instance,
    float value)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        CurrentAO->COV_Increment = value;
        Analog_Output_COV_Detect(index, Analog_Output_Present_Value(index));
    }
}

float Analog_Output_Present_Value(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    float value = ANALOG_RELINQUISH_DEFAULT;
    unsigned index = 0; /* offset from instance lookup */
    unsigned i = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        /* When all the priorities are level null, the present value returns */
        /* the Relinquish Default value */
        value = CurrentAO->Relinquish_Default;
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (CurrentAO->Priority_Array[i] != ANALOG_LEVEL_NULL) {
                value = CurrentAO->Priority_Array[i];
                break;
            }
        }
    }

    return value;
}

unsigned Analog_Output_Present_Value_Priority(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* instance to index conversion */
    unsigned i = 0;     /* loop counter */
    unsigned priority = 0;      /* return value */

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (CurrentAO->Priority_Array[priority] != ANALOG_LEVEL_NULL) {
                priority = i + 1;
                break;
            }
        }
    }

    return priority;
}

/* Set */
bool Analog_Output_Present_Value_Set(
    uint32_t object_instance,
    float value,
    uint8_t priority)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        if (priority && (priority <= BACNET_MAX_PRIORITY) &&
            (priority != 6 /* reserved */ ) ) {
            //CurrentAO->Present_Value = value;
            CurrentAO->Priority_Array[priority - 1] = value;
            /* Note: you could set the physical output here to the next
               highest priority, or to the relinquish default if no
               priorities are set.
               However, if Out of Service is TRUE, then don't set the
               physical output.  This comment may apply to the
               main loop (i.e. check out of service before changing output) */
            if (priority == 8) {
                CurrentAO->Priority_Array[15] = value;
            }
            Analog_Output_COV_Detect(index, Analog_Output_Present_Value(index));
            status = true;
        }
    }
    return status;
}

/* Relinquish */
bool Analog_Output_Present_Value_Relinquish(
    uint32_t object_instance,
    unsigned priority)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0;
    bool status = false;

    index = Analog_Output_Instance_To_Index(object_instance);
    if (index < max_analog_outputs_int) {
        CurrentAO = &AO_Descr[index];
        if (priority && (priority <= BACNET_MAX_PRIORITY) &&
            (priority != 6 /* reserved */ )) {
            CurrentAO->Priority_Array[priority - 1] = ANALOG_LEVEL_NULL;
            /* Note: you could set the physical output here to the next
               highest priority, or to the relinquish default if no
               priorities are set.
               However, if Out of Service is TRUE, then don't set the
               physical output.  This comment may apply to the
               main loop (i.e. check out of service before changing output) */
            Analog_Output_COV_Detect(index, Analog_Output_Present_Value(index));
            status = true;
        }
    }
    return status;
}

/* Out_Of_Service */
bool Analog_Output_Out_Of_Service(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    bool value = false;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        value = CurrentAO->Out_Of_Service;
    }

    return value;
}

void Analog_Output_Out_Of_Service_Set(
    uint32_t object_instance,
    bool value)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        CurrentAO->Out_Of_Service = value;
    }
}

void Analog_Output_Reliability_Set(
    uint32_t object_instance,
    uint8_t value)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        CurrentAO->Reliability = value;
    }
}

uint8_t Analog_Output_Reliability(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    uint8_t value = 0;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        value = CurrentAO->Reliability;
    }

    return value;
}

char *Analog_Output_Description(
    uint32_t object_instance)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    char *pName = NULL; /* return value */

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        pName = CurrentAO->Object_Description;
    }

    return pName;
}

bool Analog_Output_Description_Set(
    uint32_t object_instance,
    char *new_name)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        status = true;
        if (new_name) {
            for (i = 0; i < sizeof(CurrentAO->Object_Description); i++) {
                CurrentAO->Object_Description[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentAO->Object_Description); i++) {
                CurrentAO->Object_Description[i] = 0;
            }
        }
    }

    return status;
}

static bool Analog_Output_Description_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentAO->Object_Description)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentAO->Object_Description,
                    sizeof(CurrentAO->Object_Description),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",CurrentAO->Instance);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_ao", idx_c,
                            "description", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_ao\n");
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
bool Analog_Output_Object_Name(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING * object_name)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;
    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        status = characterstring_init_ansi(object_name, CurrentAO->Object_Name);
    }

    return status;
}

static bool Analog_Output_Object_Name_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentAO->Object_Name)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentAO->Object_Name,
                    sizeof(CurrentAO->Object_Name),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",CurrentAO->Instance);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_ao", idx_c,
                            "name", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_ao\n");
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

/* return apdu len, or BACNET_STATUS_ERROR on error */
int Analog_Output_Read_Property(
    BACNET_READ_PROPERTY_DATA * rpdata)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    int len = 0;
    int apdu_len = 0;   /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    float present_value = 0;
    unsigned index = 0;
    unsigned i = 0;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;

    if (Analog_Output_Valid_Instance(rpdata->object_instance)) {
        index = Analog_Output_Instance_To_Index(rpdata->object_instance);
        CurrentAO = &AO_Descr[index];
    } else
        return BACNET_STATUS_ERROR;

    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len =
                encode_application_object_id(&apdu[0], OBJECT_ANALOG_OUTPUT,
                rpdata->object_instance);
            break;

        case PROP_OBJECT_NAME:
            Analog_Output_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Analog_Output_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_ANALOG_OUTPUT);
            break;

        case PROP_PRESENT_VALUE:
            apdu_len =
                encode_application_real(&apdu[0],
                Analog_Output_Present_Value(rpdata->object_instance));
            break;

        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
#if defined(INTRINSIC_REPORTING)
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM,
                CurrentAO->Event_State ? true : false);
#else
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
#endif
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE,
                    CurrentAO->Out_Of_Service);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_STATE:
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentAO->Event_State);
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;

        case PROP_OUT_OF_SERVICE:
            apdu_len =
                encode_application_boolean(&apdu[0],
                Analog_Output_Out_Of_Service(rpdata->object_instance));
            break;

        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(&apdu[0], 
                CurrentAO->Reliability);
            break;

        case PROP_COV_INCREMENT:
            apdu_len = encode_application_real(&apdu[0], 
                CurrentAO->COV_Increment);
            break;

        case PROP_UNITS:
            apdu_len =
                encode_application_enumerated(&apdu[0], CurrentAO->Units);
            break;

        case PROP_PRIORITY_ARRAY:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0)
                apdu_len =
                    encode_application_unsigned(&apdu[0], BACNET_MAX_PRIORITY);
            /* if no index was specified, then try to encode the entire list */
            /* into one packet. */
            else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
                    /* FIXME: check if we have room before adding it to APDU */
                    if (CurrentAO->Priority_Array[i] == ANALOG_LEVEL_NULL)
                        len = encode_application_null(&apdu[apdu_len]);
                    else {
                        present_value = CurrentAO->Priority_Array[i];
                        len =
                            encode_application_real(&apdu[apdu_len],
                            present_value);
                    }
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
            } else {
                if (rpdata->array_index <= BACNET_MAX_PRIORITY) {
                    if (CurrentAO->Priority_Array[rpdata->array_index - 1]
                        == ANALOG_LEVEL_NULL)
                        apdu_len = encode_application_null(&apdu[0]);
                    else {
                        present_value =
                            CurrentAO->Priority_Array[rpdata->array_index - 1];
                        apdu_len =
                            encode_application_real(&apdu[0], present_value);
                    }
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            present_value = CurrentAO->Relinquish_Default;
            apdu_len = encode_application_real(&apdu[0], present_value);
            break;

        case PROP_MAX_PRES_VALUE:
            apdu_len =
                encode_application_real(&apdu[0], CurrentAO->Max_Pres_Value);
            break;

        case PROP_MIN_PRES_VALUE:
            apdu_len =
                encode_application_real(&apdu[0], CurrentAO->Min_Pres_Value);
            break;

#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            apdu_len =
                encode_application_unsigned(&apdu[0], CurrentAO->Time_Delay);
            break;

        case PROP_NOTIFICATION_CLASS:
            apdu_len =
                encode_application_unsigned(&apdu[0],
                CurrentAO->Notification_Class);
            break;

        case PROP_HIGH_LIMIT:
            apdu_len =
                encode_application_real(&apdu[0], CurrentAO->High_Limit);
            break;

        case PROP_LOW_LIMIT:
            apdu_len = encode_application_real(&apdu[0], CurrentAO->Low_Limit);
            break;

        case PROP_DEADBAND:
            apdu_len = encode_application_real(&apdu[0], CurrentAO->Deadband);
            break;

        case PROP_LIMIT_ENABLE:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, 0,
                (CurrentAO->
                    Limit_Enable & EVENT_LOW_LIMIT_ENABLE) ? true : false);
            bitstring_set_bit(&bit_string, 1,
                (CurrentAO->
                    Limit_Enable & EVENT_HIGH_LIMIT_ENABLE) ? true : false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_ENABLE:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                (CurrentAO->
                    Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                (CurrentAO->
                    Event_Enable & EVENT_ENABLE_TO_FAULT) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                (CurrentAO->
                    Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true : false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_ACKED_TRANSITIONS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                CurrentAO->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                CurrentAO->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                CurrentAO->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_NOTIFY_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentAO->Notify_Type ? NOTIFY_EVENT : NOTIFY_ALARM);
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
                        &CurrentAO->Event_Time_Stamps[i].date);
                    len +=
                        encode_application_time(&apdu[apdu_len + len],
                        &CurrentAO->Event_Time_Stamps[i].time);
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
                    &CurrentAO->Event_Time_Stamps[rpdata->array_index].date);
                apdu_len +=
                    encode_application_time(&apdu[apdu_len],
                    &CurrentAO->Event_Time_Stamps[rpdata->array_index].time);
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
bool Analog_Output_Write_Property(
    BACNET_WRITE_PROPERTY_DATA * wp_data)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    bool status = false;        /* return value */
    unsigned index = 0;
    int object_type = 0;
    uint32_t object_instance = 0;
    unsigned int priority = 0;
    uint8_t level = ANALOG_LEVEL_NULL;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    ctx = ucix_init("bacnet_ao");
    const char *idx_c;
    char cur_value[16];
    float pvalue;
    int i;
    time_t cur_value_time;
    char idx_cc[64];
    

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
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->object_property != PROP_EVENT_TIME_STAMPS) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        /*  only array properties can have array options */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    if (Analog_Output_Valid_Instance(wp_data->object_instance)) {
        index = Analog_Output_Instance_To_Index(wp_data->object_instance);
        CurrentAO = &AO_Descr[index];
        sprintf(idx_cc,"%d",CurrentAO->Instance);
        idx_c = idx_cc;
    } else
        return false;

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
                    status = Analog_Output_Object_Name_Write(
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
                status = Analog_Output_Description_Write(
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
            if (value.tag == BACNET_APPLICATION_TAG_REAL) {
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                if (Analog_Output_Present_Value_Set(wp_data->object_instance,
                        value.type.Real, wp_data->priority)) {
                    status = true;
                    sprintf(cur_value,"%f",value.type.Real);
                    ucix_add_option(ctx, "bacnet_ao", idx_c, "value",
                        cur_value);
                    cur_value_time = time(NULL);
                    ucix_add_option_int(ctx, "bacnet_ao", idx_c, "value_time",
                        cur_value_time);
                    ucix_add_option_int(ctx, "bacnet_ao", idx_c, "write",
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
                    level = ANALOG_LEVEL_NULL;
                    priority = wp_data->priority;
                    if (priority && (priority <= BACNET_MAX_PRIORITY)) {
                        priority--;
                        CurrentAO->Priority_Array[priority] = level;
                        /* Note: you could set the physical output here to the next
                           highest priority, or to the relinquish default if no
                           priorities are set.
                           However, if Out of Service is TRUE, then don't set the
                           physical output.  This comment may apply to the
                           main loop (i.e. check out of service before changing output) */
                        pvalue = CurrentAO->Relinquish_Default;
                        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
                            if (CurrentAO->Priority_Array[i] != ANALOG_LEVEL_NULL) {
                                pvalue = CurrentAO->Priority_Array[i];
                                break;
                            }
                        }
                        sprintf(cur_value,"%f",pvalue);
                        ucix_add_option(ctx, "bacnet_ao", idx_c, "value",
                            cur_value);
                        cur_value_time = time(NULL);
                        ucix_add_option_int(ctx, "bacnet_ao", idx_c, "value_time",
                            cur_value_time);
                        ucix_add_option_int(ctx, "bacnet_ao", idx_c, "write",
                            1);
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
                CurrentAO->Out_Of_Service = value.type.Boolean;
            }
            break;

        case PROP_RELIABILITY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentAO->Reliability = value.type.Enumerated;
            }
            break;

        case PROP_COV_INCREMENT:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                if (value.type.Real >= 0.0) {
                    Analog_Output_COV_Increment_Set(
                        wp_data->object_instance,
                        value.type.Real);
                } else {
                    status = false;
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            }
            break;

        case PROP_UNITS:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentAO->Units = value.type.Enumerated;
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentAO->Relinquish_Default = value.type.Real;
            }
            break;

        case PROP_MAX_PRES_VALUE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentAO->Max_Pres_Value = value.type.Real;
                ucix_add_option_int(ctx, "bacnet_ao", idx_c, "max_value",
                        value.type.Real);
            }
            break;

        case PROP_MIN_PRES_VALUE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentAO->Min_Pres_Value = value.type.Real;
                ucix_add_option_int(ctx, "bacnet_ao", idx_c, "min_value",
                        value.type.Real);
            }
            break;

#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentAO->Time_Delay = value.type.Unsigned_Int;
                CurrentAO->Remaining_Time_Delay = CurrentAO->Time_Delay;
                ucix_add_option_int(ctx, "bacnet_ao", idx_c, "time_delay",
                    value.type.Unsigned_Int);
            }
            break;

        case PROP_NOTIFICATION_CLASS:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentAO->Notification_Class = value.type.Unsigned_Int;
                ucix_add_option_int(ctx, "bacnet_ao", idx_c, "nc",
                    value.type.Unsigned_Int);
            }
            break;

        case PROP_HIGH_LIMIT:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentAO->High_Limit = value.type.Real;
                ucix_add_option_int(ctx, "bacnet_ao", idx_c, "high_limit",
                        value.type.Real);
            }
            break;

        case PROP_LOW_LIMIT:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentAO->Low_Limit = value.type.Real;
                ucix_add_option_int(ctx, "bacnet_ao", idx_c, "low_limit",
                        value.type.Real);
            }
            break;

        case PROP_DEADBAND:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentAO->Deadband = value.type.Real;
                ucix_add_option_int(ctx, "bacnet_ao", idx_c, "dead_limit",
                        value.type.Real);
            }
            break;

        case PROP_LIMIT_ENABLE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 2) {
                    CurrentAO->Limit_Enable = value.type.Bit_String.value[0];
                    ucix_add_option_int(ctx, "bacnet_ao", idx_c, "limit",
                        value.type.Bit_String.value[0]);
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    status = false;
                }
            }
            break;

        case PROP_EVENT_ENABLE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 3) {
                    CurrentAO->Event_Enable = value.type.Bit_String.value[0];
                    ucix_add_option_int(ctx, "bacnet_ao", idx_c, "event",
                        value.type.Bit_String.value[0]);
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
                        CurrentAO->Notify_Type = 1;
                        break;
                    case NOTIFY_ALARM:
                        CurrentAO->Notify_Type = 0;
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
        case PROP_PRIORITY_ARRAY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
#if defined(INTRINSIC_REPORTING)
        case PROP_ACKED_TRANSITIONS:
        case PROP_EVENT_TIME_STAMPS:
#endif
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }
    ucix_commit(ctx, "bacnet_ao");
    ucix_cleanup(ctx);
    return status;
}


void Analog_Output_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    ANALOG_OUTPUT_DESCR *CurrentAO;
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    BACNET_CHARACTER_STRING msgText;
    unsigned index = 0;
    uint8_t FromState = 0;
    uint8_t ToState;
    float ExceededLimit = 0.0f;
    float PresentVal = 0.0f;
    bool SendNotify = false;

    if (Analog_Output_Valid_Instance(object_instance)) {
        index = Analog_Output_Instance_To_Index(object_instance);
        CurrentAO = &AO_Descr[index];
    } else
        return;

    /* check limits */
    if (!CurrentAO->Limit_Enable)
        return; /* limits are not configured */


    if (CurrentAO->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        CurrentAO->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = CurrentAO->Ack_notify_data.EventState;

#if PRINT_ENABLED
        fprintf(stderr, "Send Acknotification for (%s,%d).\n",
            bactext_object_type_name(OBJECT_ANALOG_OUTPUT), object_instance);
#endif /* PRINT_ENABLED */

        characterstring_init_ansi(&msgText, "AckNotification");

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Analog_Output_Present_Value(object_instance);
        FromState = CurrentAO->Event_State;
        switch (CurrentAO->Event_State) {
            case EVENT_STATE_NORMAL:
                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the High_Limit for a minimum
                   period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-OFFNORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal > CurrentAO->High_Limit) &&
                    ((CurrentAO->Limit_Enable & EVENT_HIGH_LIMIT_ENABLE) ==
                        EVENT_HIGH_LIMIT_ENABLE) &&
                    ((CurrentAO->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                        EVENT_ENABLE_TO_OFFNORMAL)) {
                    if (!CurrentAO->Remaining_Time_Delay)
                        CurrentAO->Event_State = EVENT_STATE_HIGH_LIMIT;
                    else
                        CurrentAO->Remaining_Time_Delay--;
                    break;
                }

                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the Low_Limit plus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (b) the LowLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-NORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal < CurrentAO->Low_Limit) &&
                    ((CurrentAO->Limit_Enable & EVENT_LOW_LIMIT_ENABLE) ==
                        EVENT_LOW_LIMIT_ENABLE) &&
                    ((CurrentAO->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                        EVENT_ENABLE_TO_OFFNORMAL)) {
                    if (!CurrentAO->Remaining_Time_Delay)
                        CurrentAO->Event_State = EVENT_STATE_LOW_LIMIT;
                    else
                        CurrentAO->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                CurrentAO->Remaining_Time_Delay = CurrentAO->Time_Delay;
                break;

            case EVENT_STATE_HIGH_LIMIT:
                /* Once exceeded, the Present_Value must fall below the High_Limit minus
                   the Deadband before a TO-NORMAL event is generated under these conditions:
                   (a) the Present_Value must fall below the High_Limit minus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-NORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal < CurrentAO->High_Limit - CurrentAO->Deadband)
                    && ((CurrentAO->Limit_Enable & EVENT_HIGH_LIMIT_ENABLE) ==
                        EVENT_HIGH_LIMIT_ENABLE) &&
                    ((CurrentAO->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                        EVENT_ENABLE_TO_NORMAL)) {
                    if (!CurrentAO->Remaining_Time_Delay)
                        CurrentAO->Event_State = EVENT_STATE_NORMAL;
                    else
                        CurrentAO->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                CurrentAO->Remaining_Time_Delay = CurrentAO->Time_Delay;
                break;

            case EVENT_STATE_LOW_LIMIT:
                /* Once the Present_Value has fallen below the Low_Limit,
                   the Present_Value must exceed the Low_Limit plus the Deadband
                   before a TO-NORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the Low_Limit plus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (b) the LowLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-NORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal > CurrentAO->Low_Limit + CurrentAO->Deadband)
                    && ((CurrentAO->Limit_Enable & EVENT_LOW_LIMIT_ENABLE) ==
                        EVENT_LOW_LIMIT_ENABLE) &&
                    ((CurrentAO->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                        EVENT_ENABLE_TO_NORMAL)) {
                    if (!CurrentAO->Remaining_Time_Delay)
                        CurrentAO->Event_State = EVENT_STATE_NORMAL;
                    else
                        CurrentAO->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                CurrentAO->Remaining_Time_Delay = CurrentAO->Time_Delay;
                break;

            default:
                return; /* shouldn't happen */
        }       /* switch (FromState) */

        ToState = CurrentAO->Event_State;

        if (FromState != ToState) {
            /* Event_State has changed.
               Need to fill only the basic parameters of this type of event.
               Other parameters will be filled in common function. */

            switch (ToState) {
                case EVENT_STATE_HIGH_LIMIT:
                    ExceededLimit = CurrentAO->High_Limit;
                    characterstring_init_ansi(&msgText, "Goes to high limit");
                    break;

                case EVENT_STATE_LOW_LIMIT:
                    ExceededLimit = CurrentAO->Low_Limit;
                    characterstring_init_ansi(&msgText, "Goes to low limit");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_HIGH_LIMIT) {
                        ExceededLimit = CurrentAO->High_Limit;
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from high limit");
                    } else {
                        ExceededLimit = CurrentAO->Low_Limit;
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from low limit");
                    }
                    break;

                default:
                    ExceededLimit = 0;
                    break;
            }   /* switch (ToState) */

#if PRINT_ENABLED
            fprintf(stderr, "Event_State for (%s,%d) goes from %s to %s.\n",
                bactext_object_type_name(OBJECT_ANALOG_OUTPUT), object_instance,
                bactext_event_state_name(FromState),
                bactext_event_state_name(ToState));
#endif /* PRINT_ENABLED */

            /* Notify Type */
            event_data.notifyType = CurrentAO->Notify_Type;

            /* Send EventNotification. */
            SendNotify = true;
        }
    }


    if (SendNotify) {
        /* Event Object Identifier */
        event_data.eventObjectIdentifier.type = OBJECT_ANALOG_OUTPUT;
        event_data.eventObjectIdentifier.instance = object_instance;

        /* Time Stamp */
        event_data.timeStamp.tag = TIME_STAMP_DATETIME;
        Device_getCurrentDateTime(&event_data.timeStamp.value.dateTime);

        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* fill Event_Time_Stamps */
            switch (ToState) {
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                    CurrentAO->Event_Time_Stamps[TRANSITION_TO_OFFNORMAL] =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    CurrentAO->Event_Time_Stamps[TRANSITION_TO_FAULT] =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentAO->Event_Time_Stamps[TRANSITION_TO_NORMAL] =
                        event_data.timeStamp.value.dateTime;
                    break;
            }
        }

        /* Notification Class */
        event_data.notificationClass = CurrentAO->Notification_Class;

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
        event_data.toState = CurrentAO->Event_State;

        /* Event Values */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* Value that exceeded a limit. */
            event_data.notificationParams.outOfRange.exceedingValue =
                PresentVal;
            /* Status_Flags of the referenced object. */
            bitstring_init(&event_data.notificationParams.outOfRange.
                statusFlags);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_IN_ALARM,
                CurrentAO->Event_State ? true : false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_OUT_OF_SERVICE,
                CurrentAO->Out_Of_Service);
            /* Deadband used for limit checking. */
            event_data.notificationParams.outOfRange.deadband =
                CurrentAO->Deadband;
            /* Limit that was exceeded. */
            event_data.notificationParams.outOfRange.exceededLimit =
                ExceededLimit;
        }

        /* add data from notification class */
        Notification_Class_common_reporting_function(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) &&
            (event_data.ackRequired == true)) {
            switch (event_data.toState) {
                case EVENT_STATE_OFFNORMAL:
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                    CurrentAO->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                        bIsAcked = false;
                    CurrentAO->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                        Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    CurrentAO->Acked_Transitions[TRANSITION_TO_FAULT].
                        bIsAcked = false;
                    CurrentAO->Acked_Transitions[TRANSITION_TO_FAULT].
                        Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentAO->Acked_Transitions[TRANSITION_TO_NORMAL].
                        bIsAcked = false;
                    CurrentAO->Acked_Transitions[TRANSITION_TO_NORMAL].
                        Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;
            }
        }
    }
#endif /* defined(INTRINSIC_REPORTING) */
}


#if defined(INTRINSIC_REPORTING)
int Analog_Output_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    //ANALOG_OUTPUT_DESCR *CurrentAO;
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;


    /* check index */
    if (Analog_Output_Valid_Instance(index)) {
        /* Event_State not equal to NORMAL */
        IsActiveEvent = (AO_Descr[index].Event_State != EVENT_STATE_NORMAL);

        /* Acked_Transitions property, which has at least one of the bits
           (TO-OFFNORMAL, TO-FAULT, TONORMAL) set to FALSE. */
        IsNotAckedTransitions =
            (AO_Descr[index].Acked_Transitions[TRANSITION_TO_OFFNORMAL].
            bIsAcked ==
            false) | (AO_Descr[index].Acked_Transitions[TRANSITION_TO_FAULT].
            bIsAcked ==
            false) | (AO_Descr[index].Acked_Transitions[TRANSITION_TO_NORMAL].
            bIsAcked == false);
    } else
        return -1;      /* end of list  */

    if ((IsActiveEvent) || (IsNotAckedTransitions)) {
        /* Object Identifier */
        getevent_data->objectIdentifier.type = OBJECT_ANALOG_OUTPUT;
        getevent_data->objectIdentifier.instance =
            Analog_Output_Index_To_Instance(index);
        /* Event State */
        getevent_data->eventState = AO_Descr[index].Event_State;
        /* Acknowledged Transitions */
        bitstring_init(&getevent_data->acknowledgedTransitions);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_OFFNORMAL,
            AO_Descr[index].Acked_Transitions[TRANSITION_TO_OFFNORMAL].
            bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_FAULT,
            AO_Descr[index].Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_NORMAL,
            AO_Descr[index].Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);
        /* Event Time Stamps */
        for (i = 0; i < 3; i++) {
            getevent_data->eventTimeStamps[i].tag = TIME_STAMP_DATETIME;
            getevent_data->eventTimeStamps[i].value.dateTime =
                AO_Descr[index].Event_Time_Stamps[i];
        }
        /* Notify Type */
        getevent_data->notifyType = AO_Descr[index].Notify_Type;
        /* Event Enable */
        bitstring_init(&getevent_data->eventEnable);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_OFFNORMAL,
            (AO_Descr[index].
                Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true : false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_FAULT,
            (AO_Descr[index].
                Event_Enable & EVENT_ENABLE_TO_FAULT) ? true : false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_NORMAL,
            (AO_Descr[index].
                Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true : false);
        /* Event Priorities */
        Notification_Class_Get_Priorities(AO_Descr[index].Notification_Class,
            getevent_data->eventPriorities);

        return 1;       /* active event */
    } else
        return 0;       /* no active event at this index */
}

int Analog_Output_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    ANALOG_OUTPUT_DESCR *CurrentAO;
    unsigned index = 0;

    if (Analog_Output_Valid_Instance(alarmack_data->eventObjectIdentifier.
        instance)) {
        index =
            Analog_Output_Instance_To_Index(alarmack_data->eventObjectIdentifier.
            instance);
        CurrentAO = &AO_Descr[index];
    } else {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }

    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
        case EVENT_STATE_HIGH_LIMIT:
        case EVENT_STATE_LOW_LIMIT:
            if (CurrentAO->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                bIsAcked == false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentAO->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentAO->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                    bIsAcked = true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_FAULT:
            if (CurrentAO->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentAO->
                        Acked_Transitions[TRANSITION_TO_FAULT].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentAO->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_NORMAL:
            if (CurrentAO->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentAO->
                        Acked_Transitions[TRANSITION_TO_NORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentAO->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
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
    CurrentAO->Ack_notify_data.bSendAckNotify = true;
    CurrentAO->Ack_notify_data.EventState = alarmack_data->eventStateAcked;

    /* Return OK */
    return 1;
}

int Analog_Output_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    //ANALOG_OUTPUT_DESCR *CurrentAO;

    /* check index */
    if (index < max_analog_outputs_int) {
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((AO_Descr[index].Event_State != EVENT_STATE_NORMAL) &&
            (AO_Descr[index].Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = OBJECT_ANALOG_OUTPUT;
            getalarm_data->objectIdentifier.instance =
                Analog_Output_Index_To_Instance(index);
            /* Alarm State */
            getalarm_data->alarmState = AO_Descr[index].Event_State;
            /* Acknowledged Transitions */
            bitstring_init(&getalarm_data->acknowledgedTransitions);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_OFFNORMAL,
                AO_Descr[index].Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_FAULT,
                AO_Descr[index].
                Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_NORMAL,
                AO_Descr[index].
                Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);

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
    bool bResult;

    /*
     * start out assuming success and only set up error
     * response if validation fails.
     */
    bResult = true;
    if (pValue->tag != ucExpectedTag) {
        bResult = false;
        *pErrorClass = ERROR_CLASS_PROPERTY;
        *pErrorCode = ERROR_CODE_INVALID_DATA_TYPE;
    }

    return (bResult);
}

void testAnalog_Output(
    Test * pTest)
{
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint32_t decoded_instance = 0;
    uint16_t decoded_type = 0;
    BACNET_READ_PROPERTY_DATA rpdata;

    Analog_Output_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_ANALOG_OUTPUT;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Analog_Output_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_ANALOG_OUTPUT
int main(
    void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Analog Output", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testAnalog_Output);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void) ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_ANALOG_OUTPUT */
#endif /* TEST */
