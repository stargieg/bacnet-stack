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

/* Binary Output Objects customize for your use */

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
#include "bo.h"
#include "ucix.h"

/* number of demo objects */
#ifndef MAX_BINARY_OUTPUTS
#define MAX_BINARY_OUTPUTS 1024
#endif
unsigned max_binary_outputs_int = 0;

/* When all the priorities are level null, the present value returns */
/* the Relinquish Default value */
#define BINARY_RELINQUISH_DEFAULT 0

/* we choose to have a NULL level in our system represented by */
/* a particular value.  When the priorities are not in use, they */
/* will be relinquished (i.e. set to the NULL level). */
#define BINARY_LEVEL_NULL 255

BINARY_OUTPUT_DESCR BO_Descr[MAX_BINARY_OUTPUTS];

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Binary_Output_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,
    PROP_STATUS_FLAGS,
    PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,
    PROP_POLARITY,
    PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
    -1
};

static const int Binary_Output_Properties_Optional[] = {
    PROP_DESCRIPTION,
    PROP_ACTIVE_TEXT,
    PROP_INACTIVE_TEXT,
#if defined(INTRINSIC_REPORTING)
    PROP_ALARM_VALUE,
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

static const int Binary_Output_Properties_Proprietary[] = {
    -1
};

struct uci_context *ctx;

void Binary_Output_Property_Lists(
    const int **pRequired,
    const int **pOptional,
    const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Binary_Output_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Binary_Output_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Binary_Output_Properties_Proprietary;
    }

    return;
}

void Binary_Output_Load_UCI_List(const char *sec_idx,
	struct bo_inst_itr_ctx *itr)
{
	bo_inst_tuple_t *t = malloc(sizeof(bo_inst_tuple_t));
	bool disable;
	disable = ucix_get_option_int(itr->ctx, itr->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx,"default") == 0)
		return;
	if (disable)
		return;
	if( (t = (bo_inst_tuple_t *)malloc(sizeof(bo_inst_tuple_t))) != NULL ) {
		strncpy(t->idx, sec_idx, sizeof(t->idx));
		t->next = itr->list;
		itr->list = t;
	}
    return;
}

/*
 * Things to do when starting up the stack for Binary Output.
 * Should be called whenever we reset the device or power it up
 */
void Binary_Output_Init(
    void)
{
    unsigned i, j;
    static bool initialized = false;
    char name[64];
    const char *uciname;
    int ucidisable;
    int ucivalue;
    char description[64];
    const char *ucidescription;
    const char *ucidescription_default;
    const char *idx_c;
    char idx_cc[64];
    char inactive_text[64];
    const char *uciinactive_text;
    const char *uciinactive_text_default;
    char active_text[64];
    const char *uciactive_text;
    const char *uciactive_text_default;
#if defined(INTRINSIC_REPORTING)
    int ucinc_default;
    int ucinc;
    int ucievent_default;
    int ucievent;
    int ucitime_delay_default;
    int ucitime_delay;
    int ucifb_value_default;
    int ucifb_value;
#endif
    int ucipolarity_default;
    int ucipolarity;
    const char *sec = "bacnet_bo";

	char *section;
	char *type;
	struct bo_inst_itr_ctx itr_m;
	section = "bacnet_bo";

#if PRINT_ENABLED
    fprintf(stderr, "Binary_Output_Init\n");
#endif
    if (!initialized) {
        initialized = true;
        ctx = ucix_init("bacnet_bo");
#if PRINT_ENABLED
        if(!ctx)
            fprintf(stderr,  "Failed to load config file bacnet_bo\n");
#endif
		type = "bo";
		bo_inst_tuple_t *cur = malloc(sizeof (bo_inst_tuple_t));
		itr_m.list = NULL;
		itr_m.section = section;
		itr_m.ctx = ctx;
		ucix_for_each_section_type(ctx, section, type,
			(void *)Binary_Output_Load_UCI_List, &itr_m);

        ucidescription_default = ucix_get_option(ctx, sec, "default",
            "description");
        uciinactive_text_default = ucix_get_option(ctx, sec, "default",
            "inactive");
        uciactive_text_default = ucix_get_option(ctx, sec, "default",
            "active");
#if defined(INTRINSIC_REPORTING)
        ucinc_default = ucix_get_option_int(ctx, sec, "default",
            "nc", -1);
        ucievent_default = ucix_get_option_int(ctx, sec, "default",
            "event", -1);
        ucitime_delay_default = ucix_get_option_int(ctx, sec, "default",
            "time_delay", -1);
        ucifb_value_default = ucix_get_option_int(ctx, sec, "default",
            "fb_value", -1);
#endif
        ucipolarity_default = ucix_get_option_int(ctx, sec, "default",
            "polarity", 0);
        i = 0;
		for( cur = itr_m.list; cur; cur = cur->next ) {
			strncpy(idx_cc, cur->idx, sizeof(idx_cc));
            idx_c = idx_cc;
            uciname = ucix_get_option(ctx, "bacnet_bo", idx_c, "name");
            ucidisable = ucix_get_option_int(ctx, "bacnet_bo", idx_c,
                "disable", 0);
            if ((uciname != 0) && (ucidisable == 0)) {
                memset(&BO_Descr[i], 0x00, sizeof(BINARY_OUTPUT_DESCR));
                /* initialize all the priority arrays to NULL */
                for (j = 0; j < BACNET_MAX_PRIORITY; j++) {
                    BO_Descr[i].Priority_Array[j] = BINARY_LEVEL_NULL;
                }
                BO_Descr[i].Instance=atoi(idx_cc);
                BO_Descr[i].Disable=false;
                sprintf(name, "%s", uciname);
                ucix_string_copy(BO_Descr[i].Object_Name,
                    sizeof(BO_Descr[i].Object_Name), name);
                ucidescription = ucix_get_option(ctx, "bacnet_bo",
                    idx_c, "description");
                if (ucidescription != 0) {
                    sprintf(description, "%s", ucidescription);
                } else if (ucidescription_default != 0) {
                    sprintf(description, "%s %lu", ucidescription_default,
                        (unsigned long) i);
                } else {
                    sprintf(description, "BO%lu no uci section configured",
                        (unsigned long) i);
                }
                ucix_string_copy(BO_Descr[i].Object_Description,
                    sizeof(BO_Descr[i].Object_Description), description);

                uciinactive_text = ucix_get_option(ctx, "bacnet_bo",
                    idx_c, "inactive");
                if (uciinactive_text != 0) {
                    sprintf(inactive_text, "%s", uciinactive_text);
                } else if (uciinactive_text_default != 0) {
                    sprintf(inactive_text, "%s", uciinactive_text_default);
                } else {
                    sprintf(inactive_text, "inactive");
                }
                characterstring_init_ansi(&BO_Descr[i].Inactive_Text, inactive_text);

                uciactive_text = ucix_get_option(ctx, "bacnet_bo",
                    idx_c, "active");
                if (uciactive_text != 0) {
                    sprintf(active_text, "%s", uciactive_text);
                } else if (uciactive_text_default != 0) {
                    sprintf(active_text, "%s", uciactive_text_default);
                } else {
                    sprintf(active_text, "active");
                }
                characterstring_init_ansi(&BO_Descr[i].Active_Text, active_text);
   
                ucivalue = ucix_get_option_int(ctx, "bacnet_bo", idx_c,
                    "value", 0);
                BO_Descr[i].Priority_Array[15] = ucivalue;

                BO_Descr[i].Relinquish_Default = 0; //TODO read uci

                ucipolarity = ucix_get_option_int(ctx, "bacnet_bo", idx_c,
                    "polarity", ucipolarity_default);

                BO_Descr[i].Polarity = ucipolarity;


#if defined(INTRINSIC_REPORTING)
                ucinc = ucix_get_option_int(ctx, "bacnet_bo", idx_c,
                    "nc", ucinc_default);
                ucievent = ucix_get_option_int(ctx, "bacnet_bo", idx_c,
                    "event", ucievent_default);
                ucitime_delay = ucix_get_option_int(ctx, "bacnet_bo", idx_c,
                    "time_delay", ucitime_delay_default);
                ucifb_value = ucix_get_option_int(ctx, "bacnet_bo", idx_c,
                    "fb_value", ucifb_value_default);
                BO_Descr[i].Feedback_Value = ucifb_value;
                BO_Descr[i].Event_State = EVENT_STATE_NORMAL;
                /* notification class not connected */
                if (ucinc > -1) BO_Descr[i].Notification_Class = ucinc;
                else BO_Descr[i].Notification_Class = BACNET_MAX_INSTANCE;
                if (ucievent > -1) BO_Descr[i].Event_Enable = ucievent;
                else BO_Descr[i].Event_Enable = 0;
                if (ucitime_delay > -1) BO_Descr[i].Time_Delay = ucitime_delay;
                else BO_Descr[i].Time_Delay = 0;
                if (ucifb_value > -1) BO_Descr[i].Feedback_Value = ucifb_value;
                else BO_Descr[i].Feedback_Value = 0;
                /* initialize Event time stamps using wildcards
                   and set Acked_transitions */
                for (j = 0; j < MAX_BACNET_EVENT_TRANSITION; j++) {
                    datetime_wildcard_set(&BO_Descr[i].Event_Time_Stamps[j]);
                    BO_Descr[i].Acked_Transitions[j].bIsAcked = true;
                }
        
                /* Set handler for GetEventInformation function */
                handler_get_event_information_set(OBJECT_BINARY_OUTPUT,
                    Binary_Output_Event_Information);
                /* Set handler for AcknowledgeAlarm function */
                handler_alarm_ack_set(OBJECT_BINARY_OUTPUT,
                    Binary_Output_Alarm_Ack);
                /* Set handler for GetAlarmSummary Service */
                handler_get_alarm_summary_set(OBJECT_BINARY_OUTPUT,
                    Binary_Output_Alarm_Summary);
#endif
                i++;
                max_binary_outputs_int = i;
            }
        }
#if PRINT_ENABLED
        fprintf(stderr, "max_binary_outputs %i\n", max_binary_outputs_int);
#endif
        if(ctx)
            ucix_cleanup(ctx);
    }
    return;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Binary_Output_Instance_To_Index(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    int index,i;
    index = max_binary_outputs_int;
    for (i = 0; i < index; i++) {
    	CurrentBO = &BO_Descr[i];
    	if (CurrentBO->Instance == object_instance) {
    		return i;
    	}
    }
    return MAX_BINARY_OUTPUTS;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Binary_Output_Index_To_Instance(
    unsigned index)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    uint32_t instance;
	CurrentBO = &BO_Descr[index];
	instance = CurrentBO->Instance;
	return instance;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Binary_Output_Count(
    void)
{
    return max_binary_outputs_int;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Binary_Output_Valid_Instance(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    index = Binary_Output_Instance_To_Index(object_instance);
    if (index == MAX_BINARY_OUTPUTS) {
#if PRINT_ENABLED
        fprintf(stderr, "Binary_Output_Valid_Instance %i invalid index %i max %i\n",
            object_instance,index,max_binary_outputs_int);
#endif
    	return false;
    }
    CurrentBO = &BO_Descr[index];
    if (CurrentBO->Disable == false)
            return true;

    return false;
}

BACNET_BINARY_PV Binary_Output_Present_Value(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    BACNET_BINARY_PV value = BINARY_RELINQUISH_DEFAULT;
    unsigned index = 0;
    unsigned i = 0;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        /* When all the priorities are level null, the present value returns */
        /* the Relinquish Default value */
        value = CurrentBO->Relinquish_Default;
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (CurrentBO->Priority_Array[i] != BINARY_LEVEL_NULL) {
                value = CurrentBO->Priority_Array[i];
                break;
            }
        }
    }

    return value;
}

unsigned Binary_Output_Present_Value_Priority(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* instance to index conversion */
    unsigned i = 0;     /* loop counter */
    unsigned priority = 0;      /* return value */

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (CurrentBO->Priority_Array[priority] != BINARY_LEVEL_NULL) {
                priority = i + 1;
                break;
            }
        }
    }

    return priority;
}



bool Binary_Output_Out_Of_Service(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    bool value = false;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        value = CurrentBO->Out_Of_Service;
    }

    return value;
}

void Binary_Output_Out_Of_Service_Set(
    uint32_t object_instance,
    bool value)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        if (CurrentBO->Out_Of_Service != value) {
            CurrentBO->Change_Of_Value = true;
        }
        CurrentBO->Out_Of_Service = value;
    }

    return;
}

uint8_t Binary_Output_Reliability(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    uint8_t value = 0;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        value = CurrentBO->Reliability;
    }

    return value;
}

void Binary_Output_Reliability_Set(
    uint32_t object_instance,
    uint8_t value)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        CurrentBO->Reliability = value;
    }
}

static char *Binary_Output_Description(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    char *pName = NULL; /* return value */

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        pName = CurrentBO->Object_Description;
    }

    return pName;
}

bool Binary_Output_Description_Set(
    uint32_t object_instance,
    char *new_name)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        status = true;
        if (new_name) {
            for (i = 0; i < sizeof(CurrentBO->Object_Description); i++) {
                CurrentBO->Object_Description[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentBO->Object_Description); i++) {
                CurrentBO->Object_Description[i] = 0;
            }
        }
    }

    return status;
}

static bool Binary_Output_Description_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentBO->Object_Description)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentBO->Object_Description,
                    sizeof(CurrentBO->Object_Description),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",index);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_bo", idx_c,
                            "description", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_bo\n");
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
bool Binary_Output_Object_Name(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING * object_name)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        status = characterstring_init_ansi(object_name, CurrentBO->Object_Name);
    }

    return status;
}

/* note: the object name must be unique within this device */
bool Binary_Output_Name_Set(
    uint32_t object_instance,
    char *new_name)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        status = true;
        /* FIXME: check to see if there is a matching name */
        if (new_name) {
            for (i = 0; i < sizeof(CurrentBO->Object_Name); i++) {
                CurrentBO->Object_Name[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentBO->Object_Name); i++) {
                CurrentBO->Object_Name[i] = 0;
            }
        }
    }

    return status;
}

static bool Binary_Output_Object_Name_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentBO->Object_Name)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentBO->Object_Name,
                    sizeof(CurrentBO->Object_Name),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",index);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_bo", idx_c,
                            "name", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_bo\n");
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

bool Binary_Output_Change_Of_Value(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    bool status = false;
    unsigned index = 0;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        status = CurrentBO->Change_Of_Value;
    }

    return status;
}

void Binary_Output_Change_Of_Value_Clear(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        CurrentBO->Change_Of_Value = false;
    }
}

/* returns true if value has changed */
bool Binary_Output_Encode_Value_List(
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
            Binary_Output_Present_Value(object_instance);
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
        if (Binary_Output_Out_Of_Service(object_instance)) {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, true);
        } else {
            bitstring_set_bit(&value_list->value.type.Bit_String,
                STATUS_FLAG_OUT_OF_SERVICE, false);
        }
        value_list->priority = BACNET_NO_PRIORITY;
    }
    status = Binary_Output_Change_Of_Value(object_instance);

    return status;
}

bool Binary_Output_Present_Value_Set(
    uint32_t object_instance,
    BACNET_BINARY_PV value,
    uint8_t priority)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;
    if (value > 1)
        value = BINARY_LEVEL_NULL;
    
    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        if (priority && (priority <= BACNET_MAX_PRIORITY) &&
            (priority != 6 /* reserved */ ) && (value > 0)) {
#if defined(INTRINSIC_REPORTING)
            CurrentBO->Feedback_Value = (uint8_t) value;
#endif
            CurrentBO->Priority_Array[priority - 1] = (uint8_t) value;
            if (priority == 8) {
                CurrentBO->Priority_Array[15] = (uint8_t) value;
            }
            status = true;
        }
    }
    return status;
}

BACNET_POLARITY Binary_Output_Polarity(
    uint32_t object_instance)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */

    BACNET_POLARITY polarity = POLARITY_NORMAL;

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        polarity = CurrentBO->Polarity;
    }

    return polarity;
}

bool Binary_Output_Polarity_Set(
    uint32_t object_instance,
    BACNET_POLARITY polarity)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;
    const char *idx_c;
    char idx_cc[64];

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        CurrentBO->Polarity=polarity;
        sprintf(idx_cc,"%d",index);
        idx_c = idx_cc;
        if(ctx) {
            ucix_add_option_int(ctx, "bacnet_bo", idx_c,
                "polarity", polarity);
#if PRINT_ENABLED
        } else {
            fprintf(stderr,
                "Failed to open config file bacnet_bo\n");
#endif
        }
    }

    return status;
}

static bool Binary_Output_Active_Text_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentBO->Active_Text)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_copy(
                    &CurrentBO->Active_Text, char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",index);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_bo", idx_c,
                            "active", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_bo\n");
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

static bool Binary_Output_Inactive_Text_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Binary_Output_Valid_Instance(object_instance)) {
        index = Binary_Output_Instance_To_Index(object_instance);
        CurrentBO = &BO_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentBO->Inactive_Text)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_copy(
                    &CurrentBO->Inactive_Text, char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",index);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_bo", idx_c,
                            "inactive", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_bo\n");
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


/* return apdu length, or BACNET_STATUS_ERROR on error */
int Binary_Output_Read_Property(
    BACNET_READ_PROPERTY_DATA * rpdata)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    int len = 0;
    int apdu_len = 0;   /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    BACNET_BINARY_PV present_value = 0;
    unsigned index = 0;
    unsigned i = 0;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    
    if (Binary_Output_Valid_Instance(rpdata->object_instance)) {
        index = Binary_Output_Instance_To_Index(rpdata->object_instance);
        CurrentBO = &BO_Descr[index];
    } else
        return BACNET_STATUS_ERROR;

    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len =
                encode_application_object_id(&apdu[0], OBJECT_BINARY_OUTPUT,
                rpdata->object_instance);
            break;

        case PROP_OBJECT_NAME:
            /* note: object name must be unique in our device */
            Binary_Output_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Binary_Output_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], OBJECT_BINARY_OUTPUT);
            break;

        case PROP_PRESENT_VALUE:
            /* note: you need to look up the actual value */
            apdu_len =
                encode_application_enumerated(&apdu[0],
                Binary_Output_Present_Value(rpdata->object_instance));
            break;

        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
#if defined(INTRINSIC_REPORTING)
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM,
                CurrentBO->Event_State ? true : false);
#else
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
#endif
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE,
                    CurrentBO->Out_Of_Service);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentBO->Event_State);
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;

        case PROP_OUT_OF_SERVICE:
            apdu_len =
                encode_application_boolean(&apdu[0],
                Binary_Output_Out_Of_Service(rpdata->object_instance));
            break;

        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(&apdu[0], 
                CurrentBO->Reliability);
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
                    if (CurrentBO->Priority_Array[i] == BINARY_LEVEL_NULL) {
                        len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        present_value = CurrentBO->Priority_Array[i];
                        len =
                            encode_application_enumerated(&apdu[apdu_len],
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
                    if (CurrentBO->Priority_Array[rpdata->array_index - 1]
                        == BINARY_LEVEL_NULL)
                        apdu_len = encode_application_null(&apdu[0]);
                    else {
                        present_value =
                            CurrentBO->Priority_Array[rpdata->array_index - 1];
                        apdu_len =
                            encode_application_enumerated(&apdu[0],present_value);
                    }
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            present_value = CurrentBO->Relinquish_Default;
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;

        case PROP_POLARITY:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                Binary_Output_Polarity(rpdata->object_instance));
            break;

#if defined(INTRINSIC_REPORTING)
        case PROP_FEEDBACK_VALUE:
            len =
                encode_application_enumerated(&apdu[apdu_len],
                CurrentBO->Feedback_Value);
                apdu_len += len;
            break;

        case PROP_TIME_DELAY:
            apdu_len =
                encode_application_unsigned(&apdu[0], CurrentBO->Time_Delay);
            break;

        case PROP_NOTIFICATION_CLASS:
            apdu_len =
                encode_application_unsigned(&apdu[0],
                CurrentBO->Notification_Class);
            break;

        case PROP_EVENT_ENABLE:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                (CurrentBO->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true :
                false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                (CurrentBO->Event_Enable & EVENT_ENABLE_TO_FAULT) ? true :
                false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                (CurrentBO->Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true :
                false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_ACKED_TRANSITIONS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                CurrentBO->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                CurrentBO->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                CurrentBO->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_NOTIFY_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentBO->Notify_Type ? NOTIFY_EVENT : NOTIFY_ALARM);
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
                        &CurrentBO->Event_Time_Stamps[i].date);
                    len +=
                        encode_application_time(&apdu[apdu_len + len],
                        &CurrentBO->Event_Time_Stamps[i].time);
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
                    &CurrentBO->Event_Time_Stamps[rpdata->array_index].date);
                apdu_len +=
                    encode_application_time(&apdu[apdu_len],
                    &CurrentBO->Event_Time_Stamps[rpdata->array_index].time);
                apdu_len +=
                    encode_closing_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
            } else {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
            break;
#endif
        case PROP_ACTIVE_TEXT:
            apdu_len =
                encode_application_character_string(&apdu[0], &CurrentBO->Active_Text);
            break;
        case PROP_INACTIVE_TEXT:
            apdu_len =
                encode_application_character_string(&apdu[0], &CurrentBO->Inactive_Text);
            break;

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
bool Binary_Output_Write_Property(
    BACNET_WRITE_PROPERTY_DATA * wp_data)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    bool status = false;        /* return value */
    unsigned int index = 0;
    int object_type = 0;
    uint32_t object_instance = 0;
    unsigned int priority = 0;
    uint8_t level = BINARY_LEVEL_NULL;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    ctx = ucix_init("bacnet_bo");
#if defined(INTRINSIC_REPORTING)
    const char index_c[32] = "";
#endif
    const char *idx_c;
    BACNET_BINARY_PV pvalue = 0;
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
    /*  only array properties can have array options */
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }

    if (Binary_Output_Valid_Instance(wp_data->object_instance)) {
        index = Binary_Output_Instance_To_Index(wp_data->object_instance);
        CurrentBO = &BO_Descr[index];
        sprintf(idx_cc,"%d",CurrentBO->Instance);
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
                    status = Binary_Output_Object_Name_Write(
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
                status = Binary_Output_Description_Write(
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
            if (value.tag == BACNET_APPLICATION_TAG_BOOLEAN || 
                value.tag == BACNET_APPLICATION_TAG_ENUMERATED || 
                value.tag == BACNET_APPLICATION_TAG_UNSIGNED_INT) {
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                if (Binary_Output_Present_Value_Set(wp_data->object_instance,
                        value.type.Unsigned_Int, wp_data->priority)) {
                    status = true;
                    ucix_add_option_int(ctx, "bacnet_bo", idx_c, "value",
                        value.type.Unsigned_Int);
                    cur_value_time = time(NULL);
                    ucix_add_option_int(ctx, "bacnet_bo", idx_c, "value_time",
                        cur_value_time);
                    ucix_add_option_int(ctx, "bacnet_bo", idx_c, "write",
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
                    level = BINARY_LEVEL_NULL;
                    priority = wp_data->priority;
                    if (priority && (priority <= BACNET_MAX_PRIORITY)) {
                        priority--;
                        CurrentBO->Priority_Array[priority] = level;
                        /* Note: you could set the physical output here to the next
                           highest priority, or to the relinquish default if no
                           priorities are set.
                           However, if Out of Service is TRUE, then don't set the
                           physical output.  This comment may apply to the
                           main loop (i.e. check out of service before changing output) */
                        pvalue = CurrentBO->Relinquish_Default;
                        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
                            if (CurrentBO->Priority_Array[i] != BINARY_LEVEL_NULL) {
                                pvalue = CurrentBO->Priority_Array[i];
                                break;
                            }
                        }
                        ucix_add_option_int(ctx, "bacnet_bo", idx_c, "value",
                            pvalue);
                        cur_value_time = time(NULL);
                        ucix_add_option_int(ctx, "bacnet_bo", idx_c, "value_time",
                            cur_value_time);
                        ucix_add_option_int(ctx, "bacnet_bo", idx_c, "write",
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
                Binary_Output_Out_Of_Service_Set(wp_data->object_instance,
                    value.type.Boolean);
            }
            break;

        case PROP_RELIABILITY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentBO->Reliability = value.type.Enumerated;
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentBO->Relinquish_Default = value.type.Enumerated;
            }
            break;

        case PROP_POLARITY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                if (value.type.Enumerated < MAX_POLARITY) {
                    Binary_Output_Polarity_Set(wp_data->object_instance,
                        (BACNET_POLARITY) value.type.Enumerated);
                } else {
                    status = false;
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            }
            break;
#if defined(INTRINSIC_REPORTING)
        case PROP_FEEDBACK_VALUE:
            if (value.tag == BACNET_APPLICATION_TAG_ENUMERATED) {
                if (wp_data->array_index == 0) {
                    /* Array element zero is the number of
                       elements in the array.  We have a fixed
                       size array, so we are read-only. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                    BACNET_BINARY_PV fb_value = 0;
                    int array_index = 0;
                    fb_value = wp_data->application_data[array_index];
                    CurrentBO->Feedback_Value = fb_value;
                    ucix_add_option_int(ctx, "bacnet_bo", idx_c, "fb_value",
                        fb_value);
                }
            }
        case PROP_TIME_DELAY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentBO->Time_Delay = value.type.Unsigned_Int;
                CurrentBO->Remaining_Time_Delay = CurrentBO->Time_Delay;
                ucix_add_option_int(ctx, "bacnet_bo", index_c, "time_delay", value.type.Unsigned_Int);
            }
            break;

        case PROP_NOTIFICATION_CLASS:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentBO->Notification_Class = value.type.Unsigned_Int;
                ucix_add_option_int(ctx, "bacnet_bo", index_c, "nc", value.type.Unsigned_Int);
            }
            break;

        case PROP_EVENT_ENABLE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 3) {
                    CurrentBO->Event_Enable = value.type.Bit_String.value[0];
                    ucix_add_option_int(ctx, "bacnet_bo", index_c, "event", value.type.Bit_String.value[0]);
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
                        CurrentBO->Notify_Type = 1;
                        break;
                    case NOTIFY_ALARM:
                        CurrentBO->Notify_Type = 0;
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
        case PROP_ACTIVE_TEXT:
            if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                status = Binary_Output_Active_Text_Write(
                    wp_data->object_instance,
                    &value.type.Character_String,
                    &wp_data->error_class,
                    &wp_data->error_code);
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;

        case PROP_INACTIVE_TEXT:
            if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                status = Binary_Output_Inactive_Text_Write(
                    wp_data->object_instance,
                    &value.type.Character_String,
                    &wp_data->error_class,
                    &wp_data->error_code);
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;

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
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
#endif
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }
    ucix_commit(ctx, "bacnet_bo");
    ucix_cleanup(ctx);
    return status;
}

void Binary_Output_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    BINARY_OUTPUT_DESCR *CurrentBO;
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    BACNET_CHARACTER_STRING msgText;
    unsigned int index;
    uint8_t FromState = 0;
    uint8_t ToState;
    uint8_t PresentVal = 0;
    bool SendNotify = false;
    bool tonormal = true;

    index = Binary_Output_Instance_To_Index(object_instance);
    if (index < max_binary_outputs_int)
        CurrentBO = &BO_Descr[index];
    else
        return;

    if (CurrentBO->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        CurrentBO->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = CurrentBO->Ack_notify_data.EventState;

#if PRINT_ENABLED
        fprintf(stderr, "Send Acknotification for (%s,%d).\n",
            bactext_object_type_name(OBJECT_BINARY_OUTPUT), object_instance);
#endif /* PRINT_ENABLED */

        characterstring_init_ansi(&msgText, "AckNotification");

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Binary_Output_Present_Value(object_instance);
        FromState = CurrentBO->Event_State;
        switch (CurrentBO->Event_State) {
            case EVENT_STATE_NORMAL:
                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the High_Limit for a minimum
                   period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-OFFNORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal != CurrentBO->Feedback_Value) &&
                    ((CurrentBO->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                    EVENT_ENABLE_TO_OFFNORMAL)) {
                        if (!CurrentBO->Remaining_Time_Delay)
                            CurrentBO->Event_State = EVENT_STATE_FAULT;
                        else
                            CurrentBO->Remaining_Time_Delay--;
                        break;
                }
                /* value of the object is still in the same event state */
                CurrentBO->Remaining_Time_Delay = CurrentBO->Time_Delay;
                break;

            case EVENT_STATE_FAULT:
                if (PresentVal != CurrentBO->Feedback_Value) {
                       tonormal = false;
                }
                if ((tonormal) &&
                    ((CurrentBO->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                    EVENT_ENABLE_TO_NORMAL)) {
                    if (!CurrentBO->Remaining_Time_Delay)
                        CurrentBO->Event_State = EVENT_STATE_NORMAL;
                    else
                        CurrentBO->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                CurrentBO->Remaining_Time_Delay = CurrentBO->Time_Delay;
                break;

            default:
                return; /* shouldn't happen */
        }       /* switch (FromState) */

        ToState = CurrentBO->Event_State;

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
                bactext_object_type_name(OBJECT_BINARY_OUTPUT),
                object_instance,
                bactext_event_state_name(FromState),
                bactext_event_state_name(ToState));
#endif /* PRINT_ENABLED */

            /* Notify Type */
            event_data.notifyType = CurrentBO->Notify_Type;

            /* Send EventNotification. */
            SendNotify = true;
        }
    }


    if (SendNotify) {
        /* Event Object Identifier */
        event_data.eventObjectIdentifier.type = OBJECT_BINARY_OUTPUT;
        event_data.eventObjectIdentifier.instance = object_instance;

        /* Time Stamp */
        event_data.timeStamp.tag = TIME_STAMP_DATETIME;
        Device_getCurrentDateTime(&event_data.timeStamp.value.dateTime);

        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* fill Event_Time_Stamps */
            switch (ToState) {
                case EVENT_STATE_FAULT:
                    CurrentBO->Event_Time_Stamps[TRANSITION_TO_FAULT] =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentBO->Event_Time_Stamps[TRANSITION_TO_NORMAL] =
                        event_data.timeStamp.value.dateTime;
                    break;
            }
        }

        /* Notification Class */
        event_data.notificationClass = CurrentBO->Notification_Class;

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
        event_data.toState = CurrentBO->Event_State;

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
                CurrentBO->Event_State ? true : false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_OUT_OF_SERVICE,
                CurrentBO->Out_Of_Service);
        }

        /* add data from notification class */
        Notification_Class_common_reporting_function(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) &&
            (event_data.ackRequired == true)) {
            switch (event_data.toState) {
                case EVENT_STATE_OFFNORMAL:
                    CurrentBO->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked =
                        false;
                    CurrentBO->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    CurrentBO->
                        Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                        false;
                    CurrentBO->
                        Acked_Transitions[TRANSITION_TO_FAULT].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentBO->
                        Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
                        false;
                    CurrentBO->
                        Acked_Transitions[TRANSITION_TO_NORMAL].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;
                 case EVENT_STATE_HIGH_LIMIT:
                 case EVENT_STATE_LOW_LIMIT:
                     break;
            }
        }
    }
#endif /* defined(INTRINSIC_REPORTING) */
}


#if defined(INTRINSIC_REPORTING)
int Binary_Output_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    //BINARY_OUTPUT_DESCR *CurrentBO;
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;


    /* check index */
    if (Binary_Output_Valid_Instance(index)) {
        /* Event_State not equal to NORMAL */
        IsActiveEvent = (BO_Descr[index].Event_State != EVENT_STATE_NORMAL);

        /* Acked_Transitions property, which has at least one of the bits
           (TO-OFFNORMAL, TO-FAULT, TONORMAL) set to FALSE. */
        IsNotAckedTransitions =
            (BO_Descr[index].
            Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked ==
            false) | (BO_Descr[index].
            Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
            false) | (BO_Descr[index].
            Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked == false);
    } else
        return -1;      /* end of list  */

    if ((IsActiveEvent) || (IsNotAckedTransitions)) {
        /* Object Identifier */
        getevent_data->objectIdentifier.type = OBJECT_BINARY_OUTPUT;
        getevent_data->objectIdentifier.instance =
            Binary_Output_Index_To_Instance(index);
        /* Event State */
        getevent_data->eventState = BO_Descr[index].Event_State;
        /* Acknowledged Transitions */
        bitstring_init(&getevent_data->acknowledgedTransitions);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_OFFNORMAL,
            BO_Descr[index].
            Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_FAULT,
            BO_Descr[index].Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_NORMAL,
            BO_Descr[index].Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);
        /* Event Time Stamps */
        for (i = 0; i < 3; i++) {
            getevent_data->eventTimeStamps[i].tag = TIME_STAMP_DATETIME;
            getevent_data->eventTimeStamps[i].value.dateTime =
                BO_Descr[index].Event_Time_Stamps[i];
        }
        /* Notify Type */
        getevent_data->notifyType = BO_Descr[index].Notify_Type;
        /* Event Enable */
        bitstring_init(&getevent_data->eventEnable);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_OFFNORMAL,
            (BO_Descr[index].Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true :
            false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_FAULT,
            (BO_Descr[index].Event_Enable & EVENT_ENABLE_TO_FAULT) ? true :
            false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_NORMAL,
            (BO_Descr[index].Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true :
            false);
        /* Event Priorities */
        Notification_Class_Get_Priorities(BO_Descr[index].Notification_Class,
            getevent_data->eventPriorities);

        return 1;       /* active event */
    } else
        return 0;       /* no active event at this index */
}

int Binary_Output_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    BINARY_OUTPUT_DESCR *CurrentBO;
    unsigned int index;

    if (Binary_Output_Valid_Instance(alarmack_data->
            eventObjectIdentifier.instance)) {
        index = Binary_Output_Instance_To_Index(alarmack_data->
            eventObjectIdentifier.instance);
        CurrentBO = &BO_Descr[index];
    } else {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }

    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
            if (CurrentBO->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked == false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentBO->Acked_Transitions
                        [TRANSITION_TO_OFFNORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentBO->
                    Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked = true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_FAULT:
            if (CurrentBO->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentBO->Acked_Transitions
                        [TRANSITION_TO_NORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentBO->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_NORMAL:
            if (CurrentBO->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentBO->Acked_Transitions
                        [TRANSITION_TO_FAULT].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentBO->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
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
    CurrentBO->Ack_notify_data.bSendAckNotify = true;
    CurrentBO->Ack_notify_data.EventState = alarmack_data->eventStateAcked;

    /* Return OK */
    return 1;
}

int Binary_Output_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    BINARY_OUTPUT_DESCR *CurrentBO;

    /* check index */
    if (index < max_binary_outputs_int) {
        CurrentBO = &BO_Descr[index];
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((CurrentBO->Event_State != EVENT_STATE_NORMAL) &&
            (CurrentBO->Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = OBJECT_BINARY_OUTPUT;
            getalarm_data->objectIdentifier.instance =
                Binary_Output_Index_To_Instance(index);
            /* Alarm State */
            getalarm_data->alarmState = CurrentBO->Event_State;
            /* Acknowledged Transitions */
            bitstring_init(&getalarm_data->acknowledgedTransitions);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_OFFNORMAL,
                CurrentBO->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_FAULT,
                CurrentBO->Acked_Transitions[TRANSITION_TO_FAULT].
                bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_NORMAL,
                CurrentBO->Acked_Transitions[TRANSITION_TO_NORMAL].
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

void testBinary_Output(
    Test * pTest)
{
    BACNET_READ_PROPERTY_DATA rpdata;
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;

    Binary_Output_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_BINARY_OUTPUT;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Binary_Output_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_BINARY_OUTPUT
int main(
    void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Binary Output", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testBinary_Output);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void) ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif /* TEST_BINARY_OUTPUT */
#endif /* TEST */
