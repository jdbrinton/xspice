/*============================================================================
FILE    EVTnode_copy.c

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

    This file contains function EVTnode_copy which copies the state
    of a node structure.

INTERFACES

    void EVTnode_copy(CKTcircuit *ckt, int node_index, Evt_Node_t *from,
                      Evt_Node_t **to)

REFERENCED FILES

    None.

NON-STANDARD FEATURES

    None.

============================================================================*/

#include "CKTdefs.h"
#include "util.h"

#include "MIF.h"
#include "EVT.h"
#include "EVTudn.h"

#include "MIFproto.h"
#include "EVTproto.h"


/*
EVTnode_copy

This function copies the state of a node structure.

If the destination is NULL, it is allocated before the copy.  This is the
case when EVTiter copies a node during a transient analysis to
save the state of an element of rhsold into the node data structure
lists.

If the destination is non-NULL, only the internal elements of the node
structure are copied. This is the case when EVTbackup restores that state
of nodes that existed at a certain timestep back into rhs and rhsold.
*/


void EVTnode_copy(
    CKTcircuit    *ckt,        /* The circuit structure */
    int           node_index,  /* The node to copy */
    Evt_Node_t    *from,       /* Location to copy from */
    Evt_Node_t    **to)        /* Location to copy to */
{

    int                 i;

    int                 udn_index;
    int                 num_outputs;
    Mif_Boolean_t       invert;

    Evt_Node_Data_t     *node_data;
    Evt_Node_Info_t     **node_table;

    Evt_Node_t          *here;


    /* Get data for fast access */
    node_data = ckt->evt->data.node;
    node_table = ckt->evt->info.node_table;

    udn_index = node_table[node_index]->udn_index;
    num_outputs = node_table[node_index]->num_outputs;
    invert = node_table[node_index]->invert;


    /* If destination is not allocated, allocate it */
    /* otherwise we just copy into the node struct */
    here = *to;
    if(here == NULL) {

        /* Use allocated structure on free list if available */
        /* Otherwise, allocate a new one */
        here = node_data->free[node_index];
        if(here) {
            *to = here;
            node_data->free[node_index] = here->next;
            here->next = NULL;
        }
        else {
            here = (void *) MALLOC(sizeof(Evt_Node_t));
            *to = here;
            /* Allocate/initialize the data in the new node struct */
            if(num_outputs > 1) {
                here->output_value = (void *) MALLOC(num_outputs * sizeof(void *));
                for(i = 0; i < num_outputs; i++) {
                    (*(g_evt_udn_info[udn_index]->create))
                            ( &(here->output_value[i]) );
                }
            }
            (*(g_evt_udn_info[udn_index]->create)) ( &(here->node_value) );
            if(invert)
                (*(g_evt_udn_info[udn_index]->create)) ( &(here->inverted_value) );
        }
    }

    /* Copy the node data */
    here->op = from->op;
    here->step = from->step;
    if(num_outputs > 1) {
        for(i = 0; i < num_outputs; i++) {
            (*(g_evt_udn_info[udn_index]->copy)) (from->output_value[i],
                                                  here->output_value[i]);
        }
    }
    (*(g_evt_udn_info[udn_index]->copy)) (from->node_value, here->node_value);
    if(invert) {
        (*(g_evt_udn_info[udn_index]->copy)) (from->inverted_value,
                                              here->inverted_value);
    }

}

