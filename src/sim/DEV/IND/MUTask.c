/*
 * Copyright (c) 1985 Thomas L. Quarles
 */

#include "prefix.h"
#include <stdio.h>
#include <math.h>
#include "INDdefs.h"
#include "IFsim.h"
#include "CKTdefs.h"
#include "SPerror.h"
#include "suffix.h"

RCSID("MUTask.c $Revision: 1.1 $ on $Date: 90/10/11 12:43:10 $")

#ifdef MUTUAL

/*ARGSUSED*/
int
MUTask(ckt,inst,which,value,select)
    CKTcircuit *ckt;
    GENinstance *inst;
    int which;
    IFvalue *value;
    IFvalue *select;
{
    MUTinstance *here = (MUTinstance*)inst;
    double vr;
    double vi;
    double sr;
    double si;
    double vm;
    switch(which) {
        case MUT_COEFF:
            value->rValue = here->MUTfactor;
            return(OK);
        case MUT_IND1:
            value->uValue = here->MUTindName1;
            return(OK);
        case MUT_IND2:
            value->uValue = here->MUTindName2;
            return(OK);
        case MUT_QUEST_SENS_DC:
            if(ckt->CKTsenInfo){
                value->rValue = *(ckt->CKTsenInfo->SEN_Sap[select->iValue + 1]+
                        here->MUTsenParmNo);
            }
            return(OK);
        case MUT_QUEST_SENS_REAL:
            if(ckt->CKTsenInfo){
                value->rValue = *(ckt->CKTsenInfo->SEN_RHS[select->iValue + 1]+
                        here->MUTsenParmNo);
            }
            return(OK);
        case MUT_QUEST_SENS_IMAG:
            if(ckt->CKTsenInfo){
                value->rValue = *(ckt->CKTsenInfo->SEN_iRHS[select->iValue + 1]+
                        here->MUTsenParmNo);
            }
            return(OK);
        case MUT_QUEST_SENS_MAG:
            if(ckt->CKTsenInfo){
                vr = *(ckt->CKTrhsOld + select->iValue + 1); 
                vi = *(ckt->CKTirhsOld + select->iValue + 1); 
                vm = sqrt(vr*vr + vi*vi);
                if(vm == 0){
                    value->rValue = 0;
                    return(OK);
                }
                sr = *(ckt->CKTsenInfo->SEN_RHS[select->iValue + 1]+
                        here->MUTsenParmNo);
                si = *(ckt->CKTsenInfo->SEN_iRHS[select->iValue + 1]+
                        here->MUTsenParmNo);
                value->rValue = (vr * sr + vi * si)/vm;
            }
            return(OK);
        case MUT_QUEST_SENS_PH:
            if(ckt->CKTsenInfo){
                vr = *(ckt->CKTrhsOld + select->iValue + 1); 
                vi = *(ckt->CKTirhsOld + select->iValue + 1); 
                vm = vr*vr + vi*vi;
                if(vm == 0){
                    value->rValue = 0;
                    return(OK);
                }
                sr = *(ckt->CKTsenInfo->SEN_RHS[select->iValue + 1]+
                        here->MUTsenParmNo);
                si = *(ckt->CKTsenInfo->SEN_iRHS[select->iValue + 1]+
                        here->MUTsenParmNo);
                value->rValue = (vr * si - vi * sr)/vm;
            }
            return(OK);
        case MUT_QUEST_SENS_CPLX:
            if(ckt->CKTsenInfo){
                value->cValue.real= 
                        *(ckt->CKTsenInfo->SEN_RHS[select->iValue + 1]+
                        here->MUTsenParmNo);
                value->cValue.imag= 
                        *(ckt->CKTsenInfo->SEN_iRHS[select->iValue + 1]+
                        here->MUTsenParmNo);
            }
            return(OK);
        default:
            return(E_BADPARM);
    }
    /* NOTREACHED */
}
#endif /* MUTUAL */
