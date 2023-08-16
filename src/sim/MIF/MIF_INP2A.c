/*============================================================================
FILE    MIF_INP2A.c

MEMBER OF process XSPICE

Copyright 1991
Georgia Tech Research Corporation
Atlanta, Georgia 30332
All Rights Reserved

PROJECT A-8503

AUTHORS

    9/12/91  Bill Kuhn

MODIFICATIONS

    <date> <person name> <nature of modifications>

SUMMARY

    This file contains the main routine for parsing code model lines
    in the SPICE circuit description input deck.

INTERFACES

    MIF_INP2A()

REFERENCED FILES

    None.

NON-STANDARD FEATURES

    None.

============================================================================*/

#include "prefix.h"
#include <stdio.h>
#include "util.h"
#include "IFsim.h"
#include "INPdefs.h"
#include "DEVdefs.h"
#include "INPmacs.h"
#include "FTEextern.h"

#include "MIFproto.h"
#include "MIFparse.h"
#include "MIFdefs.h"
#include "MIFcmdat.h"

#include "EVT.h"
#include "EVTproto.h"

#include "suffix.h"



#define NUM_SPICE3_MODELS 23   /* Number of Berkeley models in DEVices array. */
                               /* See CKT/SPIinit.c                           */


extern  SPICEdev  *DEVices[];   /* info about all device types */
extern  int       DEVmaxnum;    /* size of DEVices array       */


static void  MIFinit_inst(MIFmodel *mdfast, MIFinstance *fast);

static void  MIFget_port_type(
    GENERIC          *ckt,      /* circuit structure to put mod/inst structs in */
    INPtables        *tab,      /* symbol table for node names, etc.            */
    card             *current,  /* MUST be named 'current' for spice macros     */
    char             **line,
    char             **next_token,
    Mif_Token_Type_t *next_token_type,
    Mif_Port_Type_t  *port_type,
    char             **port_type_str,
    Mif_Conn_Info_t  *conn_info,  /* for faster access to conn info struct */
    Mif_Status_t     *status);


static void MIFget_port(
    GENERIC          *ckt,      /* circuit structure to put mod/inst structs in */
    INPtables        *tab,      /* symbol table for node names, etc.            */
    card             *current,  /* MUST be named 'current' for spice macros     */
    MIFinstance      *fast,     /* pointer to instance struct */
    char             **line,
    char             **next_token,
    Mif_Token_Type_t *next_token_type,
    Mif_Port_Type_t  def_port_type,
    char             *def_port_type_str,
    Mif_Conn_Info_t  *conn_inf,   /* for faster access to conn info struct */
    int              conn_num,
    int              port_num,
    Mif_Status_t     *status);




/* ********************************************************************* */



/*
MIF_INP2A

This function is called by INPpas2() in SPICE to parse the new
``a'' type element cards and build the required circuit structures
for the associated code model device and instance.  It first
checks the model name at the end of the element card to be sure
the model was found in pass 1 of the parser.  If so, MIFgetMod is
called to process the .model card, creating the necessary
internal model structure and filling in the parameter value
information.  Next, the instance structure is allocated. 
Finally, the connections on the element card are scanned and the
connection information is filled-in on the instance structure,
and error checks are performed.
*/


void
MIF_INP2A(ckt,tab,current)

GENERIC      *ckt;      /* circuit structure to put mod/inst structs in */
INPtables    *tab;      /* symbol table for node names, etc.            */
card         *current;  /* the card we are to parse                     */
                        /* Must be called "current" for compatibility   */
                        /* with macros                                  */
{

/* parse a code model instance card */
/* Aname <connection list> <mname> */

    char    *line;      /* the text line for this card */
    char    *name;      /* the name of the instance */
    char    *model;     /* the name of the model */

    char    *def_port_type_str;  /* The default port type in string form */
    char    *next_token;         /* a token string */

    int     i;          /* a loop counter */
    int     j;          /* a loop counter */
    int     type;       /* the type of the model for this instance */
    int     num_conn;   /* number of connections for this model */
    int     error;      /* for the IFC macro */

    MIFmodel     *mdfast;  /* pointer to model struct */
    MIFinstance  *fast;    /* pointer to instance struct */

    INPmodel  *thismodel;  /* pointer to model struct */

    Mif_Conn_Info_t  *conn_info;  /* for faster access to conn info struct */
    Mif_Param_Info_t  *param_info;  /* for faster access to param info struct */
    Mif_Port_Type_t  def_port_type;  /* the default port type */
    Mif_Status_t     status;         /* return status */
    Mif_Token_Type_t next_token_type; /* the type of the next token */


    /* get the line text from the card struct */

    line = current->line;


    /* get the name of the instance and add it to the symbol table */

    name = MIFgettok(&line);
    INPinsert(&name, tab);


    /* locate the last token on the line and put it into "model" */

    while(*line != '\0')
        model = MIFgettok(&line);

    if(model == NULL) {
        LITERR("Missing model on A type device");
        return;
    }

    /* Locate model from pass 1.  If it hasn't been processed yet, */
    /* allocate a structure in ckt for it, process its parameters  */
    /* and return a pointer to its structure in 'thismodel'        */

    current->error = MIFgetMod(ckt, model, &thismodel, tab);

    if(current->error) {
        return;
    }


    /* get the integer index into the DEVices data array for this  */
    /* model                                                       */

    type = thismodel->INPmodType;

    if((type >= DEVmaxnum) || (type < NUM_SPICE3_MODELS)) {
        LITERR("Invalid model type for A type device");
        return;
    }

    /* create a new structure for this instance in ckt */

    mdfast = thismodel->INPmodfast;
    IFC(newInstance, (ckt, mdfast, &fast, name))


    /* initialize the code model specific elements of the inst struct */

    MIFinit_inst(mdfast, fast);


    /* *********************** */
    /* Process the connections */
    /* *********************** */

    /* get the line text from the card struct */
    /* skipping over the name of the instance */
    /* and reading the first token following  */

    line = current->line;
    MIFgettok(&line);
    next_token = MIFget_token(&line,&next_token_type);


    /* loop through the fixed number of connections expected */

    for(i = 0; i < DEVices[type]->DEVpublic.num_conn; i++) {

        /* there better be at least one more token besides the model name */
        if(*line == '\0') {
            LITERR("Missing connections on A device");
            return;
        }

        /* prepare a pointer for fast access to info about this connection */
        conn_info = &(DEVices[type]->DEVpublic.conn[i]);

        /* get the default port type for this connection */
        def_port_type = conn_info->default_port_type;
        def_port_type_str = conn_info->default_type;

        /* set analog and event_driven flags on instance and model */
        if((def_port_type == MIF_DIGITAL) || (def_port_type == MIF_USER_DEFINED)) {
            fast->event_driven = MIF_TRUE;
            mdfast->event_driven = MIF_TRUE;
        }
        else {
            fast->analog = MIF_TRUE;
            mdfast->analog = MIF_TRUE;
        }

        /* check for a null connection and continue to next connection if found */

        if(next_token_type == MIF_NULL_TOK) {

            /* make sure null is allowed */
            if(! conn_info->null_allowed) {
                LITERR("NULL connection found where not allowed");
                return;
            }

            /* set the null flag to true */
            fast->conn[i]->is_null = MIF_TRUE;
            fast->conn[i]->size = 0;

            /* eat the null token and continue to next connection */
            next_token = MIFget_token(&line,&next_token_type);
            continue;
        }
        else {
            /* set the null flag to false */
            fast->conn[i]->is_null = MIF_FALSE;
        }

        /* process connection as appropriate for scalar or array */

        if(! conn_info->is_array) {     /* a scalar connection - the simpler case */

            /* do a couple of error checks */
            if(next_token_type == MIF_LARRAY_TOK) {
                LITERR("ERROR - Scalar connection expected, [ found");
                return;
            }
            if(next_token_type == MIF_RARRAY_TOK) {
                LITERR("ERROR - Unexpected ]");
                return;
            }

            /* If all OK, get the port data into the instance struct */
            /* allocating the port member of the instance struct as needed */
            MIFget_port(ckt,
                        tab,
                        current,
                        fast,
                        &line,
                        &next_token,
                        &next_token_type,
                        def_port_type,
                        def_port_type_str,
                        conn_info,
                        i,                  /* connection index */
                        0,                  /* port index for scalar connection */
                        &status);

            if(status == MIF_ERROR)
                return;

            fast->conn[i]->size = 1;
        }
        else {  /* the connection is an array - much to be done ... */

            /* get the leading port type for the array if any */
            /* it will distribute across all ports inside the braces */
            /* overriding the default type in the interface spec */

            if(next_token_type == MIF_PERCENT_TOK) {

                /* get the port type identifier and check it for validity */
                next_token = MIFget_token(&line,&next_token_type);
                MIFget_port_type(ckt,
                                 tab,
                                 current,
                                 &line,
                                 &next_token,
                                 &next_token_type,
                                 &def_port_type,
                                 &def_port_type_str,
                                 conn_info,
                                 &status);
                if(status == MIF_ERROR)
                    return;
            }

            /* check for required leading array delim character and eat it if found */
            if(next_token_type != MIF_LARRAY_TOK) {
                LITERR("Missing [, an array connection was expected");
                return;
            }
            else
                next_token = MIFget_token(&line,&next_token_type);

            /* get and process ports until ] is encountered */

            for(j = 0;
                (next_token_type != MIF_RARRAY_TOK) &&
                (*line != '\0');
                j++) {

                /* First, do some error checks */

                /* check for required leading array delim character */
                if(next_token_type == MIF_LARRAY_TOK) {
                    LITERR("ERROR - Unexpected [ - Arrays of arrays not allowed");
                    return;
                }

                /* If all OK, get the port nodes into the instance struct */
                /* allocating the port member of the instance struct as needed */

                MIFget_port(ckt,
                            tab,
                            current,
                            fast,
                            &line,
                            &next_token,
                            &next_token_type,
                            def_port_type,
                            def_port_type_str,
                            conn_info,
                            i,                  /* connection index */
                            j,                  /* port index */
                            &status);

                if(status == MIF_ERROR)
                    return;
            }

            /* make sure we exited because the end of the array connection */
            /* was reached.  If so, eat the closing array delimiter            */
            if(*line == '\0') {
                LITERR("Missing ] in array connection");
                return;
            }
            else
                next_token = MIFget_token(&line,&next_token_type);

            /* record the number of ports found for this connection */
            if(j < 1) {
                LITERR("Array connection must have at least one port");
                return;
            }
            fast->conn[i]->size = j;

        }  /* array connection processing */

        /* be careful about putting stuff here, there is a 'continue' used */
        /* in the processing of NULL connections above                     */

    } /* for number of connections */


    /* *********************** */
    /*      Error Checks       */
    /* *********************** */

    /* check for too many connections */

    if(*line != '\0') {
        LITERR("Too many connections");
        return;
    }

    /* check connection constraints */

    for(i = 0; i < DEVices[type]->DEVpublic.num_conn; i++) {

        conn_info = &(DEVices[type]->DEVpublic.conn[i]);

        if( (fast->conn[i]->is_null) &&
            (! conn_info->null_allowed) ) {
            LITERR("Null found for connection where not allowed");
            return;
        }

        if(conn_info->has_lower_bound) {
            if(fast->conn[i]->size < conn_info->lower_bound) {
                LITERR("Too few ports in connection");
                return;
            }
        }

        if(conn_info->has_upper_bound) {
            if(fast->conn[i]->size > conn_info->upper_bound) {
                LITERR("Too many ports in connection");
                return;
            }
        }
    }

    /* check model parameter constraints */
    /* some of these should probably be done in MIFgetMod() */
    /* to prevent multiple error messages                   */

    for(i = 0; i < DEVices[type]->DEVpublic.num_param; i++) {

        param_info = &(DEVices[type]->DEVpublic.param[i]);

        if(mdfast->param[i]->is_null) {
            if(! param_info->has_default) {
                LITERR("Parameter on model has no default");
                return;
            }
            else if((param_info->is_array) && (! param_info->has_conn_ref)) {
                LITERR("Defaulted array parameter must have associated array connection");
                return;
            }
        }
        if((! mdfast->param[i]->is_null) && (param_info->is_array)) {
            if(param_info->has_conn_ref) {
                if(fast->conn[param_info->conn_ref]->size != fast->param[i]->size) {
                    LITERR("Array parameter size on model does not match connection size");
                    return;
                }
            }
        }
    }
}



/* ********************************************************************* */



/*
MIFinit_inst

This function initializes the code model specific elements of the inst struct.
*/


static void  MIFinit_inst(
    MIFmodel *mdfast,       /* The model the instance is derived from */
    MIFinstance *fast)      /* The instance to initialize */
{

    int  mod_type;  /* type of this model */

    Mif_Conn_Info_t  *conn_info;

    int     i;


    /* get an index into the DEVices information structure */

    mod_type = mdfast->MIFmodType;


    /* allocate code model connector data in instance struct */

    fast->num_conn = DEVices[mod_type]->DEVpublic.num_conn;
    fast->conn = (void *) MALLOC(fast->num_conn * sizeof(void *));

    for(i = 0; i < fast->num_conn; i++)
        fast->conn[i] = (void *) MALLOC(sizeof(Mif_Conn_Data_t));

    /* initialize code model connector data */
    for(i = 0; i < fast->num_conn; i++) {
        conn_info = &(DEVices[mod_type]->DEVpublic.conn[i]);
        fast->conn[i]->name = conn_info->name;
        fast->conn[i]->description = conn_info->description;
        fast->conn[i]->is_null = MIF_TRUE;
        fast->conn[i]->size = 0;
        fast->conn[i]->port = NULL;
        switch(conn_info->direction) {
        case MIF_INOUT:
            fast->conn[i]->is_input =  MIF_TRUE;
            fast->conn[i]->is_output = MIF_TRUE;
            break;
        case MIF_IN:
            fast->conn[i]->is_input =  MIF_TRUE;
            fast->conn[i]->is_output = MIF_FALSE;
            break;
        case MIF_OUT:
            fast->conn[i]->is_input =  MIF_FALSE;
            fast->conn[i]->is_output = MIF_TRUE;
            break;
        default:
            printf("\nERROR - Impossible direction type in MIFinit_inst\n");
            exit(1);
        }
    }


    /* allocate and copy instance variable data to the instance */

    fast->num_inst_var = DEVices[mod_type]->DEVpublic.num_inst_var;
    fast->inst_var = (void *) MALLOC(fast->num_inst_var * sizeof(void *));

    for(i = 0; i < fast->num_inst_var; i++) {

        fast->inst_var[i] = (void *) MALLOC(sizeof(Mif_Inst_Var_Data_t));

        if(DEVices[mod_type]->DEVpublic.inst_var[i].is_array) {
            fast->inst_var[i]->size = 0;
            fast->inst_var[i]->element = NULL;
            /* The code model allocates space for the data and sets the size */
        }
        else {
            fast->inst_var[i]->size = 1;
            fast->inst_var[i]->element = (void *) MALLOC(sizeof(Mif_Value_t));
        }
    }


    /* copy model parameter data to the instance */

    fast->num_param = mdfast->num_param;
    fast->param = mdfast->param;

    /* initialize any additional instance data */
    fast->initialized = MIF_FALSE;
    fast->analog = MIF_FALSE;
    fast->event_driven = MIF_FALSE;
    fast->inst_index = 0;
}



/* ********************************************************************* */



/*
MIFget_port_type

This function gets the port type identifier and checks it for validity.
*/


static void 
MIFget_port_type(
    GENERIC          *ckt,      /* circuit structure to put mod/inst structs in */
    INPtables        *tab,      /* symbol table for node names, etc.            */
    card             *current,  /* MUST be named 'current' for spice macros     */
    char             **line,
    char             **next_token,
    Mif_Token_Type_t *next_token_type,
    Mif_Port_Type_t  *port_type,
    char             **port_type_str,
    Mif_Conn_Info_t  *conn_info,  /* for faster access to conn info struct */
    Mif_Status_t     *status)
{

    Mif_Boolean_t    found_type;
    char             *temp;

    int              i;

    if(**line == '\0') {
        LITERR("Missing connections on A device");
        *status = MIF_ERROR;
        return;
    }

    if(*next_token_type != MIF_STRING_TOK) {
        LITERR("Invalid port type specifier");
        *status = MIF_ERROR;
        return;
    }

    /* OK, so get the port type string from the token and read next token */

    temp = *next_token;
    *next_token = MIFget_token(line, next_token_type);

    /* check port type for validity */

    found_type = MIF_FALSE;

    for(i = 0; i < conn_info->num_allowed_types; i++) {
        if(strcmp(temp, conn_info->allowed_type_str[i]) == 0) {
            found_type = MIF_TRUE;
            *port_type = conn_info->allowed_type[i];
            *port_type_str = temp;
            break;
        }
    }

    if(! found_type) {
        LITERR("Port type is invalid");
        *status = MIF_ERROR;
    }
    else
        *status = MIF_OK;
}




/* ********************************************************************* */


/*
MIFget_port

This function processes a port being parsed, either single ended,
or both connections of a differential.
*/




static void
MIFget_port(
    GENERIC          *ckt,      /* circuit structure to put mod/inst structs in */
    INPtables        *tab,      /* symbol table for node names, etc.            */
    card             *current,  /* MUST be named 'current' for spice macros     */
    MIFinstance      *fast,     /* pointer to instance struct */
    char             **line,
    char             **next_token,
    Mif_Token_Type_t *next_token_type,
    Mif_Port_Type_t  def_port_type,
    char             *def_port_type_str,
    Mif_Conn_Info_t  *conn_info,  /* for faster access to conn info struct */
    int              conn_num,
    int              port_num,
    Mif_Status_t     *status)

{


    CKTnode         *pos_node;          /* positive connection node */
    CKTnode         *neg_node;          /* negative connection node */

    char            *node;

    /* get the leading port type if any */

    if(*next_token_type == MIF_PERCENT_TOK) {

        /* get the port type identifier and check it for validity */
        *next_token = MIFget_token(line, next_token_type);
        MIFget_port_type(ckt,
                         tab,
                         current,
                         line,
                         next_token,
                         next_token_type,
                         &def_port_type,
                         &def_port_type_str,
                         conn_info,
                         status);

        if(*status == MIF_ERROR) {
            return;
        }
    }

    /* allocate space in the instance data struct for this port */
    if(port_num == 0) {
        fast->conn[conn_num]->port = (void *) MALLOC(sizeof(void *));
        fast->conn[conn_num]->port[0] = (void *) MALLOC(sizeof(Mif_Port_Data_t));
    }
    else {
        fast->conn[conn_num]->port = (void *) REALLOC(
               fast->conn[conn_num]->port,
               ((port_num + 1) * sizeof(void *)) );
        fast->conn[conn_num]->port[port_num] = (void *) MALLOC(sizeof(Mif_Port_Data_t));
    }


    /* store the port type information in the instance struct */
    fast->conn[conn_num]->port[port_num]->type = def_port_type;
    fast->conn[conn_num]->port[port_num]->type_str = def_port_type_str;

    /* check for a leading tilde on digital ports */
    if(*next_token_type == MIF_TILDE_TOK) {
        if((def_port_type != MIF_DIGITAL) && (def_port_type != MIF_USER_DEFINED)) {
            LITERR("ERROR - Tilde not allowed on analog nodes");
            *status = MIF_ERROR;
            return;
        }
        fast->conn[conn_num]->port[port_num]->invert = MIF_TRUE;

        /* eat the tilde and get the next token */
        *next_token = MIFget_token(line, next_token_type);
        if(**line == '\0') {
            LITERR("ERROR - Not enough ports");
            *status = MIF_ERROR;
            return;
        }
    }
    else
        fast->conn[conn_num]->port[port_num]->invert = MIF_FALSE;


    /* check for null port */
    if(*next_token_type == MIF_NULL_TOK) {

        /* make sure null is allowed */
        if(! conn_info->null_allowed) {
            LITERR("NULL connection found where not allowed");
            *status = MIF_ERROR;
            return;
        }

        /* set the (port specific) null flag to true */
        fast->conn[conn_num]->port[port_num]->is_null = MIF_TRUE;

        /* set input value to zero in case user code model refers to it */
        fast->conn[conn_num]->port[port_num]->input.rvalue = 0.0;

        /* eat the null token and return */
        *next_token = MIFget_token(line, next_token_type);
        *status = MIF_OK;
        return;
    }
    else {
        /* set the (port specific) null flag to false */
        fast->conn[conn_num]->port[port_num]->is_null = MIF_FALSE;
    }


    /* next token must be a node/instance identifier ... */
    if(*next_token_type != MIF_STRING_TOK) {
        LITERR("ERROR - Expected node/instance identifier");
        *status = MIF_ERROR;
        return;
    }

    /* Get the first connection or the voltage source name */

    switch(def_port_type) {

    case MIF_VOLTAGE:
    case MIF_DIFF_VOLTAGE:
    case MIF_CURRENT:
    case MIF_DIFF_CURRENT:
    case MIF_CONDUCTANCE:
    case MIF_DIFF_CONDUCTANCE:
    case MIF_RESISTANCE:
    case MIF_DIFF_RESISTANCE:

        /* Call the spice3c1 function to put this node in the node list in ckt */
        INPtermInsert(ckt, next_token, tab, &pos_node);
    
        /* store the equation number and node identifier */
        /* This is the equivalent of what CKTbindNode() does in 3C1 */
        fast->conn[conn_num]->port[port_num]->pos_node_str = *next_token;
        fast->conn[conn_num]->port[port_num]->smp_data.pos_node = pos_node->number;

        break;

    case MIF_VSOURCE_CURRENT:

        /* Call the spice3c1 function to put this vsource instance name in */
        /* the symbol table                                                */
        INPinsert(next_token, tab);

        /* Now record the name of the vsource instance for processing */
        /* later by MIFsetup.  This is equivalent to what INPpName    */
        /* does in 3C1.  Checking to see if the source is present in  */
        /* the circuit is deferred to MIFsetup as is done in 3C1.     */
        fast->conn[conn_num]->port[port_num]->vsource_str = *next_token;

        break;

    case MIF_DIGITAL:
    case MIF_USER_DEFINED:
        /* Insert data into event-driven info structs */
        EVTtermInsert(ckt,
                       fast,
                       *next_token,
                       def_port_type_str,
                       conn_num,
                       port_num,
                       &(current->error));
        if(current->error) {
            *status = MIF_ERROR;
            return;
        }
        break;

    default:

        /* impossible connection type */
        LITERR("INTERNAL ERROR - Impossible connection type");
        *status = MIF_ERROR;
        return;
    }

    /* get the next token */
    *next_token = MIFget_token(line, next_token_type);

    /* get other node if appropriate */
    switch(def_port_type) {

    case MIF_VOLTAGE:
    case MIF_CURRENT:
    case MIF_CONDUCTANCE:
    case MIF_RESISTANCE:
        /* These are single ended types, so default other node to ground */
        node = "0";
        INPtermInsert(ckt, &node, tab, &neg_node);
        fast->conn[conn_num]->port[port_num]->neg_node_str = node;
        fast->conn[conn_num]->port[port_num]->smp_data.neg_node = neg_node->number;
        break;

    case MIF_DIFF_VOLTAGE:
    case MIF_DIFF_CURRENT:
    case MIF_DIFF_CONDUCTANCE:
    case MIF_DIFF_RESISTANCE:
        /* These are differential types, so get the other node */
        if((**line == '\0') || (*next_token_type != MIF_STRING_TOK)) {
            LITERR("ERROR - Expected node identifier");
            *status = MIF_ERROR;
            return;
        }
        INPtermInsert(ckt, next_token, tab, &neg_node);
        fast->conn[conn_num]->port[port_num]->neg_node_str = *next_token;
        fast->conn[conn_num]->port[port_num]->smp_data.neg_node = neg_node->number;
        *next_token = MIFget_token(line, next_token_type);
        break;

    default:
        /* must be vsource name, digital, or user defined, so there is no other node */
        break;

    }

    *status = MIF_OK;
    return;
}
