/**************************************************************************
*
* Copyright (C) 2011 Krzysztof Malorny <malornykrzysztof@gmail.com>
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

/* Notification Class Objects */

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
#include "config.h"
#include "device.h"
#include "handlers.h"
#include "nc.h"
#include "ucix.h"
#include "address.h"
#include "client.h"
#include "txbuf.h"

/* number of demo objects */
#ifndef MAX_NOTIFICATION_CLASSES
#define MAX_NOTIFICATION_CLASSES 1024
#endif
unsigned max_notificaton_classes_int = 0;


#if defined(INTRINSIC_REPORTING)
static NOTIFICATION_CLASS_DESCR NC_Descr[MAX_NOTIFICATION_CLASSES];

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Notification_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_NOTIFICATION_CLASS,
    PROP_PRIORITY,
    PROP_ACK_REQUIRED,
    PROP_RECIPIENT_LIST,
    -1
};

static const int Notification_Properties_Optional[] = {
    PROP_DESCRIPTION,
    -1
};

static const int Notification_Properties_Proprietary[] = {
    -1
};

struct uci_context *ctx;

void Notification_Class_Property_Lists(
    const int **pRequired,
    const int **pOptional,
    const int **pProprietary)
{
    if (pRequired)
        *pRequired = Notification_Properties_Required;
    if (pOptional)
        *pOptional = Notification_Properties_Optional;
    if (pProprietary)
        *pProprietary = Notification_Properties_Proprietary;
    return;
}

void Notification_Class_Load_UCI_List(const char *sec_idx,
	struct nc_inst_itr_ctx *itr)
{
	nc_inst_tuple_t *t = malloc(sizeof(nc_inst_tuple_t));
	bool disable;
	disable = ucix_get_option_int(itr->ctx, itr->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx,"default") == 0)
		return;
	if (disable)
		return;
	if( (t = (nc_inst_tuple_t *)malloc(sizeof(nc_inst_tuple_t))) != NULL ) {
		strncpy(t->idx, sec_idx, sizeof(t->idx));
		t->next = itr->list;
		itr->list = t;
	}
    return;
}

/*
 * Things to do when starting up the stack for Notification Class.
 * Should be called whenever we reset the device or power it up
 */
void Notification_Class_Init(
    void)
{
    unsigned i;
    static bool initialized = false;
    char name[64];
    const char *uciname;
    int ucidisable;
    char description[64];
    const char *ucidescription;
    const char *ucidescription_default;
    const char *idx_c;
    char idx_cc[64];
    char *ucirecp[NC_MAX_RECIPIENTS];
    BACNET_DESTINATION recplist[NC_MAX_RECIPIENTS];
    int ucirecp_n = 0;
    int ucirecp_i = 0;
    char *uci_ptr;
    char *uci_ptr_a;
    char *src_ip;
    const char *src_net;
    unsigned src_port, src_port1, src_port2;
    const char *sec = "bacnet_nc";

	char *section;
	char *type;
	struct nc_inst_itr_ctx itr_m;
	section = "bacnet_nc";

#if PRINT_ENABLED
    fprintf(stderr, "Notification_Class_Init\n");
#endif
    if (!initialized) {
        initialized = true;
        ctx = ucix_init(sec);
#if PRINT_ENABLED
        if(!ctx)
            fprintf(stderr,  "Failed to load config file bacnet_nc\n");
#endif
		type = "nc";
		nc_inst_tuple_t *cur = malloc(sizeof(nc_inst_tuple_t));
		itr_m.list = NULL;
		itr_m.section = section;
		itr_m.ctx = ctx;
		ucix_for_each_section_type(ctx, section, type,
			(void *)Notification_Class_Load_UCI_List, &itr_m);

        ucidescription_default = ucix_get_option(ctx, sec, "default",
            "description");
        /* initialize all the analog output priority arrays to NULL */
        i = 0;
		for( cur = itr_m.list; cur; cur = cur->next ) {
            /* init with zeros */
            memset(&NC_Descr[i], 0x00, sizeof(NOTIFICATION_CLASS_DESCR));
			strncpy(idx_cc, cur->idx, sizeof(idx_cc));
    	    idx_c = idx_cc;
            NC_Descr[i].Instance=atoi(idx_cc);
            uciname = ucix_get_option(ctx, sec, idx_c, "name");
            ucidisable = ucix_get_option_int(ctx, sec, idx_c,
            "disable", 0);
            if ((uciname != 0) && (ucidisable == 0)) {
                memset(&NC_Descr[i], 0x00, sizeof(NOTIFICATION_CLASS_DESCR));
                NC_Descr[i].Instance=atoi(idx_cc);
                NC_Descr[i].Disable=false;
                sprintf(name, "%s", uciname);
                ucix_string_copy(NC_Descr[i].Object_Name,
                    sizeof(NC_Descr[i].Object_Name), name);
                ucidescription = ucix_get_option(ctx, sec, idx_c,
                    "description");
                if (ucidescription != 0) {
                    sprintf(description, "%s", ucidescription);
                } else if (ucidescription_default != 0) {
                    sprintf(description, "%s %lu", ucidescription_default ,
                        (unsigned long) i);
                } else {
                    sprintf(description, "NC%lu no uci section configured",
                        (unsigned long) i);
                }
                ucix_string_copy(NC_Descr[i].Object_Description,
                    sizeof(NC_Descr[i].Object_Description), description);

				/* set the basic parameters */
				NC_Descr[i].Ack_Required = 7; //TODO uci
				NC_Descr[i].Priority[TRANSITION_TO_OFFNORMAL] = 255;     /* The lowest priority for Normal message. */
				NC_Descr[i].Priority[TRANSITION_TO_FAULT] = 255; /* The lowest priority for Normal message. */
				NC_Descr[i].Priority[TRANSITION_TO_NORMAL] = 255;        /* The lowest priority for Normal message. */
				ucirecp_n = ucix_get_list(ucirecp, ctx, "bacnet_nc", idx_c,
					"recipient");
				for (ucirecp_i = 0; ucirecp_i < ucirecp_n; ucirecp_i++) {
					//BACNET_ADDRESS * src = { 0 };
					recplist[ucirecp_i].ValidDays = 127; //TODO uci bit string 1,1,1,1,1,1,1 Mo,Di,Mi,Do,Fr,Sa,So
					recplist[ucirecp_i].FromTime.hour = 0;
					recplist[ucirecp_i].FromTime.min = 0;
					recplist[ucirecp_i].FromTime.sec = 0;
					recplist[ucirecp_i].FromTime.hundredths = 0;
					recplist[ucirecp_i].ToTime.hour = 23;
					recplist[ucirecp_i].ToTime.min = 59;
					recplist[ucirecp_i].ToTime.sec = 59;
					recplist[ucirecp_i].ToTime.hundredths = 99;
					recplist[ucirecp_i].ConfirmedNotify = false;
					recplist[ucirecp_i].Recipient.RecipientType =
								RECIPIENT_TYPE_ADDRESS;
					recplist[ucirecp_i].Recipient._.Address.len = 0;
					uci_ptr = strtok(ucirecp[ucirecp_i], ",");
					src_net = uci_ptr;
					if (!src_net) {
						src_net = "65535";
					}
					recplist[ucirecp_i].Recipient._.Address.net = atoi(src_net);
					if (atoi(src_net) != 65535) {
						uci_ptr = strtok(NULL, ":");
						src_ip = uci_ptr;
						uci_ptr = strtok(NULL, "\0");
						src_port = atoi(uci_ptr);
						//src[4] = src_port;
						src_port1 = ( src_port / 256 );
						//src[5] = src_port - ( src_port1 * 256 );
						src_port2 = src_port - ( src_port1 * 256 );
						uci_ptr_a = strtok(src_ip, ".");
						//src.mac[0] =  = atoi(uci_ptr_a);
						recplist[ucirecp_i].Recipient._.Address.mac[0] = atoi(uci_ptr_a);
						uci_ptr_a = strtok(NULL, ".");
						//src.mac[1] =  = atoi(uci_ptr_a);
						recplist[ucirecp_i].Recipient._.Address.mac[1] = atoi(uci_ptr_a);
						uci_ptr_a = strtok(NULL, ".");
						//src.mac[2] =  = atoi(uci_ptr_a);
						recplist[ucirecp_i].Recipient._.Address.mac[2] = atoi(uci_ptr_a);
						uci_ptr_a = strtok(NULL, ".");
						//src.mac[3] =  = atoi(uci_ptr_a);
						recplist[ucirecp_i].Recipient._.Address.mac[3] = atoi(uci_ptr_a);
						recplist[ucirecp_i].Recipient._.Address.mac[4] = src_port1;
						recplist[ucirecp_i].Recipient._.Address.mac[5] = src_port2;
						//recplist[ucirecp_i].Recipient._.Address.mac = src;
						recplist[ucirecp_i].Recipient._.Address.mac_len = 6;
						recplist[ucirecp_i].Recipient._.Address.len = 0;
						recplist[ucirecp_i].ConfirmedNotify = false;
					}
					recplist[ucirecp_i].ProcessIdentifier = ucirecp_i;
					recplist[ucirecp_i].Transitions = 7; //bit string 1,1,1 To Alarm,To Fault,To Normal
				}
				for (ucirecp_i = 0; ucirecp_i < ucirecp_n; ucirecp_i++) {
					BACNET_ADDRESS src = { 0 };
					unsigned max_apdu = 0;
					int32_t DeviceID;
	
					NC_Descr[i].Recipient_List[ucirecp_i] =
						recplist[ucirecp_i];

					if (NC_Descr[i].Recipient_List[ucirecp_i].Recipient.
						RecipientType == RECIPIENT_TYPE_DEVICE) {
						/* copy Device_ID */
						DeviceID =
							NC_Descr[i].Recipient_List[ucirecp_i].Recipient._.
							DeviceIdentifier;
						address_bind_request(DeviceID, &max_apdu, &src);

					} else if (NC_Descr[i].Recipient_List[ucirecp_i].Recipient.
						RecipientType == RECIPIENT_TYPE_ADDRESS) {
						/* copy Address */
						src = NC_Descr[i].Recipient_List[ucirecp_i].Recipient._.Address;
						address_bind_request(BACNET_MAX_INSTANCE, &max_apdu, &src);
					}
				}
				i++;
				max_notificaton_classes_int = i;
            }
        }
#if PRINT_ENABLED
        fprintf(stderr, "max_notificaton_classes: %i\n", max_notificaton_classes_int);
#endif
        if(ctx)
            ucix_cleanup(ctx);
    }
    return;
}


/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Notification_Class_Instance_To_Index(
    uint32_t object_instance)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    int index,i;
    //int instance;
    index = max_notificaton_classes_int;
    for (i = 0; i < index; i++) {
    	CurrentNC = &NC_Descr[i];
    	//instance = CurrentNC->Instance;
    	if (CurrentNC->Instance == object_instance) {
    		return i;
    	}
    }
    return MAX_NOTIFICATION_CLASSES;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Notification_Class_Index_To_Instance(
    unsigned index)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    uint32_t instance;
	CurrentNC = &NC_Descr[index];
	instance = CurrentNC->Instance;
	return instance;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Notification_Class_Count(
    void)
{
    return max_notificaton_classes_int;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Notification_Class_Valid_Instance(
    uint32_t object_instance)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0; /* offset from instance lookup */
    index = Notification_Class_Instance_To_Index(object_instance);
    if (index == MAX_NOTIFICATION_CLASSES) {
#if PRINT_ENABLED
        fprintf(stderr, "Notification_Class_Valid_Instance %i invalid\n",object_instance);
#endif
    	return false;
    }
    CurrentNC = &NC_Descr[index];
    if (CurrentNC->Disable == false)
        return true;

    return false;
}

static char *Notification_Class_Description(
    uint32_t object_instance)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0; /* offset from instance lookup */
    char *pName = NULL; /* return value */

    if (Notification_Class_Valid_Instance(object_instance)) {
        index = Notification_Class_Instance_To_Index(object_instance);
        CurrentNC = &NC_Descr[index];
        pName = CurrentNC->Object_Description;
    }

    return pName;
}

bool Notification_Class_Description_Set(
    uint32_t object_instance,
    char *new_name)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Notification_Class_Valid_Instance(object_instance)) {
        index = Notification_Class_Instance_To_Index(object_instance);
        CurrentNC = &NC_Descr[index];
        status = true;
        if (new_name) {
            for (i = 0; i < sizeof(CurrentNC->Object_Description); i++) {
                CurrentNC->Object_Description[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentNC->Object_Description); i++) {
                CurrentNC->Object_Description[i] = 0;
            }
        }
    }

    return status;
}

static bool Notification_Class_Description_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Notification_Class_Valid_Instance(object_instance)) {
        index = Notification_Class_Instance_To_Index(object_instance);
        CurrentNC = &NC_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentNC->Object_Description)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentNC->Object_Description,
                    sizeof(CurrentNC->Object_Description),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",index);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_nc", idx_c, 
                            "description", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr, 
                            "Failed to open config file bacnet_nc\n");
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
bool Notification_Class_Object_Name(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *object_name)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0;
    bool status = false;

    if (Notification_Class_Valid_Instance(object_instance)) {
        index = Notification_Class_Instance_To_Index(object_instance);
        CurrentNC = &NC_Descr[index];
        status = characterstring_init_ansi(object_name, CurrentNC->Object_Name);
    }

    return status;
}

/* note: the object name must be unique within this device */
bool Notification_Class_Name_Set(
    uint32_t object_instance,
    char *new_name)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    if (Notification_Class_Valid_Instance(object_instance)) {
        index = Notification_Class_Instance_To_Index(object_instance);
        CurrentNC = &NC_Descr[index];
        status = true;
        /* FIXME: check to see if there is a matching name */
        if (new_name) {
            for (i = 0; i < sizeof(CurrentNC->Object_Name); i++) {
                CurrentNC->Object_Name[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentNC->Object_Name); i++) {
                CurrentNC->Object_Name[i] = 0;
            }
        }
    }

    return status;
}

static bool Notification_Class_Object_Name_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */
    const char *idx_c;
    char idx_cc[64];

    if (Notification_Class_Valid_Instance(object_instance)) {
        index = Notification_Class_Instance_To_Index(object_instance);
        CurrentNC = &NC_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentNC->Object_Name)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentNC->Object_Name,
                    sizeof(CurrentNC->Object_Name),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    sprintf(idx_cc,"%d",index);
                    idx_c = idx_cc;
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_nc", idx_c,
                            "name", char_string->value);
#if PRINT_ENABLED
                    } else {
                        fprintf(stderr,
                            "Failed to open config file bacnet_nc\n");
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



int Notification_Class_Read_Property(
    BACNET_READ_PROPERTY_DATA * rpdata)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    BACNET_CHARACTER_STRING char_string;
    BACNET_OCTET_STRING octet_string;
    BACNET_BIT_STRING bit_string;
    uint8_t *apdu = NULL;
    uint8_t u8Val;
    int idx;
    unsigned index = 0;
    int apdu_len = 0;   /* return value */


    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;

    if (Notification_Class_Valid_Instance(rpdata->object_instance)) {
        index = Notification_Class_Instance_To_Index(rpdata->object_instance);
        CurrentNC = &NC_Descr[index];
    } else
        return BACNET_STATUS_ERROR;


    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len =
                encode_application_object_id(&apdu[0],
                OBJECT_NOTIFICATION_CLASS, rpdata->object_instance);
            break;

        case PROP_OBJECT_NAME:
            Notification_Class_Object_Name(rpdata->object_instance,
                &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Notification_Class_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                OBJECT_NOTIFICATION_CLASS);
            break;

        case PROP_NOTIFICATION_CLASS:
            apdu_len +=
                encode_application_unsigned(&apdu[0], rpdata->object_instance);
            break;

        case PROP_PRIORITY:
            if (rpdata->array_index == 0)
                apdu_len += encode_application_unsigned(&apdu[0], 3);
            else {
                if (rpdata->array_index == BACNET_ARRAY_ALL) {
                    apdu_len +=
                        encode_application_unsigned(&apdu[apdu_len],
                        CurrentNC->Priority[TRANSITION_TO_OFFNORMAL]);
                    apdu_len +=
                        encode_application_unsigned(&apdu[apdu_len],
                        CurrentNC->Priority[TRANSITION_TO_FAULT]);
                    apdu_len +=
                        encode_application_unsigned(&apdu[apdu_len],
                        CurrentNC->Priority[TRANSITION_TO_NORMAL]);
                } else if (rpdata->array_index <= MAX_BACNET_EVENT_TRANSITION) {
                    apdu_len +=
                        encode_application_unsigned(&apdu[apdu_len],
                        CurrentNC->Priority[rpdata->array_index - 1]);
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = -1;
                }
            }
            break;

        case PROP_ACK_REQUIRED:
            u8Val = CurrentNC->Ack_Required;

            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                (u8Val & TRANSITION_TO_OFFNORMAL_MASKED) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                (u8Val & TRANSITION_TO_FAULT_MASKED) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                (u8Val & TRANSITION_TO_NORMAL_MASKED) ? true : false);
            /* encode bitstring */
            apdu_len +=
                encode_application_bitstring(&apdu[apdu_len], &bit_string);
            break;

        case PROP_RECIPIENT_LIST:
            /* encode all entry of Recipient_List */
            for (idx = 0; idx < NC_MAX_RECIPIENTS; idx++) {
                BACNET_DESTINATION *RecipientEntry;
                int i = 0;

                /* get pointer of current element for Recipient_List  - easier for use */
                RecipientEntry = &CurrentNC->Recipient_List[idx];
                if (RecipientEntry->Recipient.RecipientType !=
                    RECIPIENT_TYPE_NOTINITIALIZED) {
                    /* Valid Days - BACnetDaysOfWeek - [bitstring] monday-sunday */
                    u8Val = 0x01;
                    bitstring_init(&bit_string);

                    for (i = 0; i < MAX_BACNET_DAYS_OF_WEEK; i++) {
                        if (RecipientEntry->ValidDays & u8Val)
                            bitstring_set_bit(&bit_string, i, true);
                        else
                            bitstring_set_bit(&bit_string, i, false);
                        u8Val <<= 1;    /* next day */
                    }
                    apdu_len +=
                        encode_application_bitstring(&apdu[apdu_len],
                        &bit_string);

                    /* From Time */
                    apdu_len +=
                        encode_application_time(&apdu[apdu_len],
                        &RecipientEntry->FromTime);

                    /* To Time */
                    apdu_len +=
                        encode_application_time(&apdu[apdu_len],
                        &RecipientEntry->ToTime);

                    /*
                       BACnetRecipient ::= CHOICE {
                       device [0] BACnetObjectIdentifier,
                       address [1] BACnetAddress
                       } */

                    /* CHOICE - device [0] BACnetObjectIdentifier */
                    if (RecipientEntry->Recipient.RecipientType ==
                        RECIPIENT_TYPE_DEVICE) {
                        apdu_len +=
                            encode_context_object_id(&apdu[apdu_len], 0,
                            OBJECT_DEVICE,
                            RecipientEntry->Recipient._.DeviceIdentifier);
                    }
                    /* CHOICE - address [1] BACnetAddress */
                    else if (RecipientEntry->Recipient.RecipientType ==
                        RECIPIENT_TYPE_ADDRESS) {
                        /* opening tag 1 */
                        apdu_len += encode_opening_tag(&apdu[apdu_len], 1);
                        /* network-number Unsigned16, */
                        apdu_len +=
                            encode_application_unsigned(&apdu[apdu_len],
                            RecipientEntry->Recipient._.Address.net);

                        /* mac-address OCTET STRING */
                        if (RecipientEntry->Recipient._.Address.net) {
                            octetstring_init(&octet_string,
                                RecipientEntry->Recipient._.Address.adr,
                                RecipientEntry->Recipient._.Address.len);
                        } else {
                            octetstring_init(&octet_string,
                                RecipientEntry->Recipient._.Address.mac,
                                RecipientEntry->Recipient._.Address.mac_len);
                        }
                        apdu_len +=
                            encode_application_octet_string(&apdu[apdu_len],
                            &octet_string);

                        /* closing tag 1 */
                        apdu_len += encode_closing_tag(&apdu[apdu_len], 1);

                    } else {;
                    }   /* shouldn't happen */

                    /* Process Identifier - Unsigned32 */
                    apdu_len +=
                        encode_application_unsigned(&apdu[apdu_len],
                        RecipientEntry->ProcessIdentifier);

                    /* Issue Confirmed Notifications - boolean */
                    apdu_len +=
                        encode_application_boolean(&apdu[apdu_len],
                        RecipientEntry->ConfirmedNotify);

                    /* Transitions - BACnet Event Transition Bits [bitstring] */
                    u8Val = RecipientEntry->Transitions;

                    bitstring_init(&bit_string);
                    bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                        (u8Val & TRANSITION_TO_OFFNORMAL_MASKED) ? true :
                        false);
                    bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                        (u8Val & TRANSITION_TO_FAULT_MASKED) ? true : false);
                    bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                        (u8Val & TRANSITION_TO_NORMAL_MASKED) ? true : false);

                    apdu_len +=
                        encode_application_bitstring(&apdu[apdu_len],
                        &bit_string);
                }
            }
            break;

        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = -1;
            break;
    }

    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool Notification_Class_Write_Property(
    BACNET_WRITE_PROPERTY_DATA * wp_data)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    NOTIFICATION_CLASS_DESCR TmpNotify;
    BACNET_APPLICATION_DATA_VALUE value;
    uint8_t TmpPriority[MAX_BACNET_EVENT_TRANSITION];  /* BACnetARRAY[3] of Unsigned */
    bool status = false;
    int iOffset;
    uint8_t idx;
    int len = 0;
    unsigned index = 0;
    int object_type = 0;
    uint32_t object_instance = 0;
    ctx = ucix_init("bacnet_nc");
    const char *idx_c;
    char idx_cc[64];

    /* decode the some of the request */
    len =
        bacapp_decode_application_data(wp_data->application_data,
        wp_data->application_data_len, &value);
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    if ((wp_data->object_property != PROP_PRIORITY) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        /*  only array properties can have array options */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }

    if (Notification_Class_Valid_Instance(wp_data->object_instance)) {
        index = Notification_Class_Instance_To_Index(wp_data->object_instance);
        CurrentNC = &NC_Descr[index];
        sprintf(idx_cc,"%d",index);
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
                    status = Notification_Class_Object_Name_Write(
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
                status = Notification_Class_Description_Write(
                    wp_data->object_instance,
                    &value.type.Character_String,
                    &wp_data->error_class,
                    &wp_data->error_code);
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;
        case PROP_PRIORITY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (wp_data->array_index == 0) {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    status = false;
                } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                    iOffset = 0;
                    for (idx = 0; idx < MAX_BACNET_EVENT_TRANSITION; idx++)
                    {
                        len =
                            bacapp_decode_application_data(&wp_data->
                            application_data[iOffset], wp_data->application_data_len,
                            &value);
                        if ((len == 0) ||
                                (value.tag != BACNET_APPLICATION_TAG_UNSIGNED_INT))
                        {
                            /* Bad decode, wrong tag or following required parameter missing */
                            wp_data->error_class = ERROR_CLASS_PROPERTY;
                            wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                            status = false;
                            break;
                        }
                        if (value.type.Unsigned_Int > 255) {
                            wp_data->error_class = ERROR_CLASS_PROPERTY;
                            wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                            status = false;
                            break;
                        }
						TmpPriority[idx] = (uint8_t) value.type.Unsigned_Int;
                        iOffset += len;
                    }
                    if (status == true) {
                        for (idx = 0; idx < MAX_BACNET_EVENT_TRANSITION; idx++)
                            CurrentNC->Priority[idx] = TmpPriority[idx];	
                    }
                } else if (wp_data->array_index <= 3) {
                    if (value.type.Unsigned_Int > 255) {
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                        status = false;
                      }
                      else
                    CurrentNC->Priority[wp_data->array_index - 1] =
                        value.type.Unsigned_Int;
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    status = false;
                }
            }
            break;

        case PROP_ACK_REQUIRED:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 3) {
                    CurrentNC->Ack_Required =
                        value.type.Bit_String.value[0];
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            }
            break;

        case PROP_RECIPIENT_LIST:

            memset(&TmpNotify, 0x00, sizeof(NOTIFICATION_CLASS_DESCR));
            char ucirecp[NC_MAX_RECIPIENTS][64];
            int ucirecp_n = 0;
            char uci_str[64];
#if PRINT_ENABLED
            if (!ctx)
                fprintf(stderr,  "Failed to open config file bacnet_nc\n");
#endif
            idx = 0;
            iOffset = 0;
            /* decode all packed */
            while (iOffset < wp_data->application_data_len) {
                /* Decode Valid Days */
                len =
                    bacapp_decode_application_data(&wp_data->
                    application_data[iOffset], wp_data->application_data_len,
                    &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_BIT_STRING)) {
                    /* Bad decode, wrong tag or following required parameter missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }

                if (value.type.Bit_String.bits_used == MAX_BACNET_DAYS_OF_WEEK)
                    /* store value */
                    TmpNotify.Recipient_List[idx].ValidDays =
                        value.type.Bit_String.value[0];
                else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_OTHER;
                    return false;
                }

                iOffset += len;
                /* Decode From Time */
                len =
                    bacapp_decode_application_data(&wp_data->
                    application_data[iOffset], wp_data->application_data_len,
                    &value);

                if ((len == 0) || (value.tag != BACNET_APPLICATION_TAG_TIME)) {
                    /* Bad decode, wrong tag or following required parameter missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].FromTime = value.type.Time;

                iOffset += len;
                /* Decode To Time */
                len =
                    bacapp_decode_application_data(&wp_data->
                    application_data[iOffset], wp_data->application_data_len,
                    &value);

                if ((len == 0) || (value.tag != BACNET_APPLICATION_TAG_TIME)) {
                    /* Bad decode, wrong tag or following required parameter missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].ToTime = value.type.Time;

                iOffset += len;
                /* context tag [0] - Device */
                if (decode_is_context_tag(&wp_data->application_data[iOffset],
                        0)) {
                    TmpNotify.Recipient_List[idx].Recipient.RecipientType =
                        RECIPIENT_TYPE_DEVICE;
                    /* Decode Network Number */
                    len =
                        bacapp_decode_context_data(&wp_data->
                        application_data[iOffset],
                        wp_data->application_data_len, &value,
                        PROP_RECIPIENT_LIST);

                    if ((len == 0) ||
                        (value.tag != BACNET_APPLICATION_TAG_OBJECT_ID)) {
                        /* Bad decode, wrong tag or following required parameter missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                    /* store value */
                    TmpNotify.Recipient_List[idx].Recipient._.
                        DeviceIdentifier = value.type.Object_Id.instance;

                    iOffset += len;
                }
                /* opening tag [1] - Recipient */
                else if (decode_is_opening_tag_number(&wp_data->
                        application_data[iOffset], 1)) {
                    iOffset++;
                    TmpNotify.Recipient_List[idx].Recipient.RecipientType =
                        RECIPIENT_TYPE_ADDRESS;
                    /* Decode Network Number */
                    len =
                        bacapp_decode_application_data(&wp_data->
                        application_data[iOffset],
                        wp_data->application_data_len, &value);

                    if ((len == 0) ||
                        (value.tag != BACNET_APPLICATION_TAG_UNSIGNED_INT)) {
                        /* Bad decode, wrong tag or following required parameter missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                    /* store value */
                    TmpNotify.Recipient_List[idx].Recipient._.Address.net =
                        value.type.Unsigned_Int;
                    iOffset += len;
                    /* Decode Address */
                    len =
                        bacapp_decode_application_data(&wp_data->
                        application_data[iOffset],
                        wp_data->application_data_len, &value);

                    if ((len == 0) ||
                        (value.tag != BACNET_APPLICATION_TAG_OCTET_STRING)) {
                        /* Bad decode, wrong tag or following required parameter missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                    /* store value */
                    unsigned src_port,src_port1,src_port2;
                    if (TmpNotify.Recipient_List[idx].Recipient._.Address.
                        net == 0) {
                        memcpy(TmpNotify.Recipient_List[idx].Recipient._.
                            Address.mac, value.type.Octet_String.value,
                            value.type.Octet_String.length);
                        TmpNotify.Recipient_List[idx].Recipient._.Address.
                            mac_len = value.type.Octet_String.length;
                        src_port1 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.mac[4];
                        src_port2 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.mac[5];
                        src_port = ( src_port1 * 256 ) + src_port2;
                        sprintf(uci_str,  "%i,%i.%i.%i.%i:%i\n", TmpNotify.
                            Recipient_List[idx].Recipient._.Address.net,
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[0],
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[1],
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[2],
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[3],
                            src_port);
                        if(ctx) {
                                sprintf(ucirecp[ucirecp_n], "%s", uci_str);
                                ucirecp_n++;
                        }
                    } else if (TmpNotify.Recipient_List[idx].Recipient._.Address.
                        net != 65535) {
                        memcpy(TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr, value.type.Octet_String.value,
                            value.type.Octet_String.length);
                        TmpNotify.Recipient_List[idx].Recipient._.Address.len =
                            value.type.Octet_String.length;
                        src_port1 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[4];
                        src_port2 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[5];
                        src_port = ( src_port1 * 256 ) + src_port2;
                        sprintf(uci_str,  "%i,%i.%i.%i.%i:%i\n", TmpNotify.
                            Recipient_List[idx].Recipient._.Address.net,
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[0],
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[1],
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[2],
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[3], src_port);
                        if(ctx) {
                                sprintf(ucirecp[ucirecp_n], "%s", uci_str);
                                ucirecp_n++;
                        }
                    } else {
                        sprintf(uci_str,  "%i\n", TmpNotify.
                            Recipient_List[idx].Recipient._.Address.net);
                        if(ctx) {
                                sprintf(ucirecp[ucirecp_n], "%s", uci_str);
                                ucirecp_n++;
                        }
                    }

                    iOffset += len;
                    /* closing tag [1] - Recipient */
                    if (decode_is_closing_tag_number(&wp_data->
                            application_data[iOffset], 1))
                        iOffset++;
                    else {
                        /* Bad decode, wrong tag or following required parameter missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                } else {
                    /* Bad decode, wrong tag or following required parameter missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }

                /* Process Identifier */
                len =
                    bacapp_decode_application_data(&wp_data->
                    application_data[iOffset], wp_data->application_data_len,
                    &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_UNSIGNED_INT)) {
                    /* Bad decode, wrong tag or following required parameter missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].ProcessIdentifier =
                    value.type.Unsigned_Int;

                iOffset += len;
                /* Issue Confirmed Notifications */
                len =
                    bacapp_decode_application_data(&wp_data->
                    application_data[iOffset], wp_data->application_data_len,
                    &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_BOOLEAN)) {
                    /* Bad decode, wrong tag or following required parameter missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].ConfirmedNotify =
                    value.type.Boolean;

                iOffset += len;
                /* Transitions */
                len =
                    bacapp_decode_application_data(&wp_data->
                    application_data[iOffset], wp_data->application_data_len,
                    &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_BIT_STRING)) {
                    /* Bad decode, wrong tag or following required parameter missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }

                if (value.type.Bit_String.bits_used ==
                    MAX_BACNET_EVENT_TRANSITION)
                    /* store value */
                    TmpNotify.Recipient_List[idx].Transitions =
                        value.type.Bit_String.value[0];
                else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_OTHER;
                    return false;
                }
                iOffset += len;

                /* Increasing element of list */
                if (++idx >= NC_MAX_RECIPIENTS) {
                    wp_data->error_class = ERROR_CLASS_RESOURCES;
                    wp_data->error_code =
                        ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
                    return false;
                }
            }

            /* Decoded all recipient list */
            /* copy elements from temporary object */
            for (idx = 0; idx < NC_MAX_RECIPIENTS; idx++) {
                BACNET_ADDRESS src = { 0 };
                unsigned max_apdu = 0;
                int32_t DeviceID;

                CurrentNC->Recipient_List[idx] =
                    TmpNotify.Recipient_List[idx];

                if (CurrentNC->Recipient_List[idx].Recipient.
                    RecipientType == RECIPIENT_TYPE_DEVICE) {
                    /* copy Device_ID */
                    DeviceID =
                        CurrentNC->Recipient_List[idx].Recipient._.
                        DeviceIdentifier;
                    address_bind_request(DeviceID, &max_apdu, &src);

                } else if (CurrentNC->Recipient_List[idx].Recipient.
                    RecipientType == RECIPIENT_TYPE_ADDRESS) {
                    /* copy Address */
                    /* src = CurrentNC->Recipient_List[idx].Recipient._.Address; */
                    /* address_bind_request(BACNET_MAX_INSTANCE, &max_apdu, &src); */
                }
            }

            if(ctx) {
                ucix_set_list(ctx, "bacnet_nc", idx_c, "recipient",
                    ucirecp, ucirecp_n);
                ucix_commit(ctx, "bacnet_nc");
                ucix_cleanup(ctx);
            }

            status = true;

            break;

        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }
    return status;
}


void Notification_Class_Get_Priorities(
    uint32_t object_instance,
    uint32_t * pPriorityArray)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    unsigned index = 0;
    int i;

    if (Notification_Class_Valid_Instance(object_instance)) {
        index = Notification_Class_Instance_To_Index(object_instance);
        CurrentNC = &NC_Descr[index];
    } else {
        for (i = 0; i < 3; i++)
            pPriorityArray[i] = 255;
        return; /* unknown object */
    }

    for (i = 0; i < 3; i++)
        pPriorityArray[i] = CurrentNC->Priority[i];
}


static bool IsRecipientActive(
    BACNET_DESTINATION * pBacDest,
    uint8_t EventToState)
{
    BACNET_DATE_TIME DateTime;

    /* valid Transitions */
    switch (EventToState) {
        case EVENT_STATE_OFFNORMAL:
        case EVENT_STATE_HIGH_LIMIT:
        case EVENT_STATE_LOW_LIMIT:
            if (!(pBacDest->Transitions & TRANSITION_TO_OFFNORMAL_MASKED))
                return false;
            break;

        case EVENT_STATE_FAULT:
            if (!(pBacDest->Transitions & TRANSITION_TO_FAULT_MASKED))
                return false;
            break;

        case EVENT_STATE_NORMAL:
            if (!(pBacDest->Transitions & TRANSITION_TO_NORMAL_MASKED))
                return false;
            break;

        default:
            return false;       /* shouldn't happen */
    }

    /* get actual date and time */
    Device_getCurrentDateTime(&DateTime);

    /* valid Days */
    if (!((0x01 << (DateTime.date.wday - 1)) & pBacDest->ValidDays))
        return false;

    /* valid FromTime */
    if (datetime_compare_time(&DateTime.time, &pBacDest->FromTime) < 0)
        return false;

    /* valid ToTime */
    if (datetime_compare_time(&pBacDest->ToTime, &DateTime.time) < 0)
        return false;

    return true;
}


void Notification_Class_common_reporting_function(
    BACNET_EVENT_NOTIFICATION_DATA * event_data)
{
    /* Fill the parameters common for all types of events. */

    NOTIFICATION_CLASS_DESCR *CurrentNC;
    BACNET_DESTINATION *pBacDest;
    unsigned index = 0;
    uint8_t recp_index;

    if (Notification_Class_Valid_Instance(event_data->notificationClass)) {
        index = Notification_Class_Instance_To_Index(event_data->notificationClass);
        CurrentNC = &NC_Descr[index];
    } else
        return;

    /* Initiating Device Identifier */
    event_data->initiatingObjectIdentifier.type = OBJECT_DEVICE;
    event_data->initiatingObjectIdentifier.instance =
        Device_Object_Instance_Number();

    /* Priority and AckRequired */
    switch (event_data->toState) {
        case EVENT_STATE_NORMAL:
            event_data->priority =
                CurrentNC->Priority[TRANSITION_TO_NORMAL];
            event_data->ackRequired =
                (CurrentNC->
                Ack_Required & TRANSITION_TO_NORMAL_MASKED) ? true : false;
            break;

        case EVENT_STATE_FAULT:
            event_data->priority =
                CurrentNC->Priority[TRANSITION_TO_FAULT];
            event_data->ackRequired =
                (CurrentNC->
                Ack_Required & TRANSITION_TO_FAULT_MASKED) ? true : false;
            break;

        case EVENT_STATE_OFFNORMAL:
        case EVENT_STATE_HIGH_LIMIT:
        case EVENT_STATE_LOW_LIMIT:
            event_data->priority =
                CurrentNC->Priority[TRANSITION_TO_OFFNORMAL];
            event_data->ackRequired =
                (CurrentNC->Ack_Required & TRANSITION_TO_OFFNORMAL_MASKED)
                ? true : false;
            break;

        default:       /* shouldn't happen */
            break;
    }

    /* send notifications for active recipients */
    /* pointer to first recipient */
    pBacDest = &CurrentNC->Recipient_List[0];
    for (recp_index = 0; recp_index < NC_MAX_RECIPIENTS; recp_index++, pBacDest++) {
        /* check if recipient is defined */
        if (pBacDest->Recipient.RecipientType == RECIPIENT_TYPE_NOTINITIALIZED)
            break;      /* recipient doesn't defined - end of list */

        if (IsRecipientActive(pBacDest, event_data->toState) == true) {
            BACNET_ADDRESS dest;
            uint32_t device_id;
            //BACNET_ADDRESS dest_adr;
            unsigned max_apdu;
            //uint8_t i = 0;
            //uint8_t adr_len = 0;

            /* Process Identifier */
            event_data->processIdentifier = pBacDest->ProcessIdentifier;

            /* send notification */
            if (pBacDest->Recipient.RecipientType == RECIPIENT_TYPE_DEVICE) {
                /* send notification to the specified device */
                device_id = pBacDest->Recipient._.DeviceIdentifier;

                if (pBacDest->ConfirmedNotify == true)
                    Send_CEvent_Notify(device_id, event_data);
                else if (address_get_by_device(device_id, &max_apdu, &dest))
                    Send_UEvent_Notify(Handler_Transmit_Buffer, event_data,
                        &dest);
            } else if (pBacDest->Recipient.RecipientType ==
                RECIPIENT_TYPE_ADDRESS) {
                /* send notification to the address indicated */
                if (pBacDest->ConfirmedNotify == true) {
                    if (address_get_device_id(&dest, &device_id))
                        Send_CEvent_Notify(device_id, event_data);
//                    dest_adr = pBacDest->Recipient._.Address;
//                    if (address_get_device_id(&dest_adr, &device_id)) {
//                        fprintf(stderr,"device id: %i len: %i net: %i\n",
//                            device_id, dest_adr.len, dest_adr.net);
//                        fprintf(stderr,"reporting dev id ConfirmedNotify\n");
//                        Send_CEvent_Notify(device_id, event_data);
//                        dest = pBacDest->Recipient._.Address;
//                        Send_UEvent_Notify(Handler_Transmit_Buffer, event_data,
//                            &dest);
//                    } else {
//                        fprintf(stderr,"reporting dev id ConfirmedNotify false\n");
//                    }
                } else {
                    dest = pBacDest->Recipient._.Address;
                    Send_UEvent_Notify(Handler_Transmit_Buffer, event_data,
                        &dest);
                }
            }
        }
    }
}

/* This function tries to find the addresses of the defined devices. */
/* It should be called periodically (example once per minute). */
void Notification_Class_find_recipient(
    void)
{
    NOTIFICATION_CLASS_DESCR *CurrentNC;
    BACNET_DESTINATION *pBacDest;
    BACNET_ADDRESS src = { 0 };
    unsigned max_apdu = 0;
    unsigned index = 0;
    uint32_t DeviceID;
    uint8_t idx;


    for (index = 0; index < max_notificaton_classes_int;
        index++) {
        /* pointer to current notification */
        CurrentNC =
            &NC_Descr[index];
//            &NC_Descr[Notification_Class_Instance_To_Index(index)];
        /* pointer to first recipient */
        pBacDest = &CurrentNC->Recipient_List[0];
        for (idx = 0; idx < NC_MAX_RECIPIENTS; idx++, pBacDest++) {
            if (CurrentNC->Recipient_List[idx].Recipient.RecipientType ==
                RECIPIENT_TYPE_DEVICE) {
                /* Device ID */
                DeviceID =
                    CurrentNC->Recipient_List[idx].Recipient._.
                    DeviceIdentifier;
                /* Send who_ is request only when address of device is unknown. */
                if (!address_bind_request(DeviceID, &max_apdu, &src))
                    Send_WhoIs(DeviceID, DeviceID);
            } else if (CurrentNC->Recipient_List[idx].Recipient.
                RecipientType == RECIPIENT_TYPE_ADDRESS) {

            }
        }
    }
}
#endif /* defined(INTRINSIC_REPORTING) */
