/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 *                                            
 * eu_msg.c  - Message Procol Subsystem. Writen based on Analogic Devices
 *             supplied code.
 *
 * This file is part of the eagle-usb driver package.
 *
 * eagle-usb driver package is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * "eagle-usb driver package" is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: eu_msg.c,v 1.9 2004/11/01 10:33:26 sleeper Exp $
 */

#include "Adiutil.h"
#include "macros.h"
#include "eu_msg.h"
#include "eu_utils.h"
#include "debug.h"




/* -------------------------- Private Macros/Variables -------------------------- */

#define CDC_REQ_SEND_ENCAPSULATED_CMD 0x00
#define CMD_SET_BLOCK                 0x01
#define DSP_MP_TX_START               0x3FCE

#define SEARCHING   0x0
#define PASSED      0x1
#define FAILED      0x2

#define NUM_CMVS                20

#define PREAMBLE                0x535C
#define RECEIVER_MODEM          0x0000
#define SENDER_HOST             0x0010
#define SUBTYPE_MEM_REQWRITE    0x0100
#define SUBTYPE_MEM_REQREAD     0x0000
#define TYPE_MEMACCESS          0x1000

#define WRITE_TO_MODEM (RECEIVER_MODEM + SENDER_HOST + SUBTYPE_MEM_REQWRITE + TYPE_MEMACCESS)
#define READ_FRM_MODEM (RECEIVER_MODEM + SENDER_HOST + SUBTYPE_MEM_REQREAD  + TYPE_MEMACCESS)

/* for MsgSeq_SoftReset */
static eu_cmv_msg_t MsgCNTL0_RSET = { PREAMBLE, WRITE_TO_MODEM, 0xCDEF, 'C','N','T','L', 0, 0 };

/* for MsgSeq_EnaFR */
static eu_cmv_msg_t MsgOPTN0EnaFR = { PREAMBLE, WRITE_TO_MODEM, 0x1111, 'O','P','T','N', 0, 0x82800044 };

/* for MsgSeq_Retrainer */
static eu_cmv_msg_t MsgCNTL0_RSVD = { PREAMBLE, WRITE_TO_MODEM, 0xEEEE, 'C','N','T','L', 0, 1 };
static eu_cmv_msg_t MsgCNTL0_INIT = { PREAMBLE, WRITE_TO_MODEM, 0xEEEE, 'C','N','T','L', 0, 2 };
static eu_cmv_msg_t MsgINFO8a     = { PREAMBLE, READ_FRM_MODEM, 0xEEEE, 'I','N','F','O', 8, 0 };
static eu_cmv_msg_t MsgINFO10     = { PREAMBLE, READ_FRM_MODEM, 0xEEEE, 'I','N','F','O',10, 0 };
static eu_cmv_msg_t MsgOPTN0      = { PREAMBLE, WRITE_TO_MODEM, 0x1111, 'O','P','T','N', 0, 0x80020066 };
static eu_cmv_msg_t MsgOPTN2      = { PREAMBLE, WRITE_TO_MODEM, 0x2222, 'O','P','T','N', 2, 0 };
static eu_cmv_msg_t MsgOPTN4      = { PREAMBLE, WRITE_TO_MODEM, 0x3333, 'O','P','T','N', 4, 0 };
static eu_cmv_msg_t MsgOPTN7      = { PREAMBLE, WRITE_TO_MODEM, 0x2222, 'O','P','T','N', 7, 0 };
static eu_cmv_msg_t MsgOPTN3      = { PREAMBLE, WRITE_TO_MODEM, 0x2222, 'O','P','T','N', 3, 0 };
static eu_cmv_msg_t MsgOPTN5      = { PREAMBLE, WRITE_TO_MODEM, 0x2222, 'O','P','T','N', 5, 0 };
static eu_cmv_msg_t MsgOPTN6      = { PREAMBLE, WRITE_TO_MODEM, 0x2222, 'O','P','T','N', 6, 0 };
static eu_cmv_msg_t MsgOPTN15     = { PREAMBLE, WRITE_TO_MODEM, 0x2222, 'O','P','T','N',15, 0 };
static eu_cmv_msg_t MsgADPT2      = { PREAMBLE, WRITE_TO_MODEM, 0x1234, 'A','D','P','T', 2, 0 };
static eu_cmv_msg_t MsgINFO9      = { PREAMBLE, WRITE_TO_MODEM, 0x1234, 'I','N','F','O', 9, 0 };
static eu_cmv_msg_t MsgMASK8      = { PREAMBLE, WRITE_TO_MODEM, 0x1234, 'M','A','S','K', 8, 0 };
static eu_cmv_msg_t MsgMASK9      = { PREAMBLE, WRITE_TO_MODEM, 0x1234, 'M','A','S','K', 9, 0 };
static eu_cmv_msg_t MsgPSDM0      = { PREAMBLE, WRITE_TO_MODEM, 0x1234, 'P','S','D','M', 0, 0 };
static eu_cmv_msg_t MsgPFCL1      = { PREAMBLE, WRITE_TO_MODEM, 0x89AB, 'P','F','C','L', 1, 0 };

/* for MsgSeq_OpStat */
static eu_cmv_msg_t MsgSTAT0a     = { PREAMBLE, READ_FRM_MODEM, 0x0123, 'S','T','A','T', 0, 0 };
static eu_cmv_msg_t MsgDIAG2      = { PREAMBLE, READ_FRM_MODEM, 0x1111, 'D','I','A','G', 2, 0 };
static eu_cmv_msg_t MsgDIAG22     = { PREAMBLE, READ_FRM_MODEM, 0x2222, 'D','I','A','G',22, 0 };
static eu_cmv_msg_t MsgDIAG23     = { PREAMBLE, READ_FRM_MODEM, 0x3333, 'D','I','A','G',23, 0 };
static eu_cmv_msg_t MsgDIAG25     = { PREAMBLE, READ_FRM_MODEM, 0x4444, 'D','I','A','G',25, 0 };
static eu_cmv_msg_t MsgDIAG51     = { PREAMBLE, READ_FRM_MODEM, 0x5555, 'D','I','A','G',51, 0 };
static eu_cmv_msg_t MsgDIAG52     = { PREAMBLE, READ_FRM_MODEM, 0x6666, 'D','I','A','G',52, 0 };
static eu_cmv_msg_t MsgDIAG53     = { PREAMBLE, READ_FRM_MODEM, 0x7777, 'D','I','A','G',53, 0 };
static eu_cmv_msg_t MsgDIAG54     = { PREAMBLE, READ_FRM_MODEM, 0x8888, 'D','I','A','G',54, 0 };
static eu_cmv_msg_t MsgRATE0      = { PREAMBLE, READ_FRM_MODEM, 0x9999, 'R','A','T','E', 0, 0 };
static eu_cmv_msg_t MsgDIAG3      = { PREAMBLE, READ_FRM_MODEM, 0xAAAA, 'D','I','A','G', 3, 0 };
static eu_cmv_msg_t MsgDIAG47     = { PREAMBLE, READ_FRM_MODEM, 0xBBBB, 'D','I','A','G',47, 0 };
static eu_cmv_msg_t MsgDIAG49     = { PREAMBLE, READ_FRM_MODEM, 0xCCCC, 'D','I','A','G',49, 0 };
static eu_cmv_msg_t MsgINFOA      = { PREAMBLE, READ_FRM_MODEM, 0xDDDD, 'I','N','F','O',10, 0 };
static eu_cmv_msg_t MsgPSDM1      = { PREAMBLE, READ_FRM_MODEM, 0xEEEE, 'P','S','D','M', 1, 0 };
static eu_cmv_msg_t MsgINFO8b     = { PREAMBLE, READ_FRM_MODEM, 0xEEEE, 'I','N','F','O', 8, 0 };
static eu_cmv_msg_t MsgINFO14     = { PREAMBLE, READ_FRM_MODEM, 0xEEEE, 'I','N','F','O',14, 0 };

/* for MsgSeq_Stat */
static eu_cmv_msg_t MsgSTAT0b     = { PREAMBLE, READ_FRM_MODEM, 0x7654, 'S','T','A','T', 0, 0 };

/* for MSgSeq_ModemEna */
/*FLASH_ACC + ENA_MODEMREBOOT_RQ + SENDER_HOST + RECEIVER_MODEM = 0x4110 */
static eu_cmv_msg_t MsgMODEMENA   = { PREAMBLE, 0x4110, 0xDEAD, 'I','N','F','O', 0, 0 };

static eu_cmv_msg_t lcl_MsgCNTL0_RSET;
static eu_cmv_msg_t lcl_MsgOPTN0EnaFR;
static eu_cmv_msg_t lcl_MsgCNTL0_RSVD;
static eu_cmv_msg_t lcl_MsgCNTL0_INIT;
static eu_cmv_msg_t lcl_MsgINFO8a;
static eu_cmv_msg_t lcl_MsgINFO10;
static eu_cmv_msg_t lcl_MsgOPTN0;
static eu_cmv_msg_t lcl_MsgOPTN2;
static eu_cmv_msg_t lcl_MsgOPTN4;
static eu_cmv_msg_t lcl_MsgOPTN7;
static eu_cmv_msg_t lcl_MsgOPTN3;
static eu_cmv_msg_t lcl_MsgOPTN5;
static eu_cmv_msg_t lcl_MsgOPTN6;
static eu_cmv_msg_t lcl_MsgOPTN15;
static eu_cmv_msg_t lcl_MsgADPT2;
static eu_cmv_msg_t lcl_MsgINFO9;
static eu_cmv_msg_t lcl_MsgMASK8;
static eu_cmv_msg_t lcl_MsgMASK9;
static eu_cmv_msg_t lcl_MsgPSDM0;
static eu_cmv_msg_t lcl_MsgPFCL1;
static eu_cmv_msg_t lcl_MsgSTAT0a;
static eu_cmv_msg_t lcl_MsgDIAG2;
static eu_cmv_msg_t lcl_MsgDIAG22;
static eu_cmv_msg_t lcl_MsgDIAG23;
static eu_cmv_msg_t lcl_MsgDIAG25;
static eu_cmv_msg_t lcl_MsgDIAG51;
static eu_cmv_msg_t lcl_MsgDIAG52;
static eu_cmv_msg_t lcl_MsgDIAG53;
static eu_cmv_msg_t lcl_MsgDIAG54;
static eu_cmv_msg_t lcl_MsgRATE0;
static eu_cmv_msg_t lcl_MsgDIAG3;
static eu_cmv_msg_t lcl_MsgDIAG47;
static eu_cmv_msg_t lcl_MsgDIAG49;
static eu_cmv_msg_t lcl_MsgINFOA;
static eu_cmv_msg_t lcl_MsgPSDM1;
static eu_cmv_msg_t lcl_MsgINFO8b;
static eu_cmv_msg_t lcl_MsgINFO14;
static eu_cmv_msg_t lcl_MsgSTAT0b;
static eu_cmv_msg_t lcl_MsgMODEMENA;


static uint32_t MsgInitDone = FALSE;


#define ADDMSG(Seq,Msg)                                 \
    (Seq).MsgMax.Msgs[(Seq).MsgCount++] = &lcl_##Msg;   \
    eu_build_msg ( &lcl_##Msg, &(Msg) );


#define ADDMSG_IFVAR_NOTZERO(Seq,Msg,Var)       \
    if ( (Var) !=0)                             \
    {                                           \
        (Msg).data = (Var);                     \
        ADDMSG(Seq,Msg);                        \
    }

#define ADDMSG_FROM_CONFIG(Seq,Msg,Var)         \
   (Msg).data = (Var);                          \
   ADDMSG (Seq,Msg);

#define ADDMSG_FROM_TEXTFILE(Seq,Msg)                   \
   (Seq).MsgMax.Msgs[(Seq).MsgCount++] = &(Msg);        \
   if ( !EU_TEST_FLAG(ins,EU_MSG_INITIALIZED) )         \
   {                                                    \
        eu_build_msg ( &(Msg), &(Msg) );                \
   }                                                    \

#define ___swahw32(x)                                           \
({                                                              \
    uint32_t __x = (x);                                         \
    ((uint32_t)(                                                \
        (((uint32_t)(__x) & (uint32_t)0x0000ffffUL) << 16) |    \
        (((uint32_t)(__x) & (uint32_t)0xffff0000UL) >> 16) ));  \
})





/* ---------------------------- Private Declarations ---------------------------- */

static void eu_write_dsp_msg (
                              eu_instance_t *ins,
                              uint16_t       addr,
                              uint16_t      *data
                             );

static void eu_build_msg ( eu_cmv_msg_t *dest, eu_cmv_msg_t *src );
static void eu_get_cgf_values ( eu_instance_t *ins, const eu_options_t opt );

/* ----------------------------- Exported Functions ----------------------------- */




/**
 * eu_decode_msg - Decode messages coming from modem
 *
 * @msg     - storage for decoded message
 * @raw_msg - Raw message as sent by the modem
 *
 * returns a boolean
 *
 */
uint32_t eu_decode_msg ( eu_msg_t *msg, uint16_t raw_msg[8] ) 
{
    int       retcode = TRUE;
    uint16_t *pword   = NULL;
    uint16_t  tmp     = 0;
    
    pword = &raw_msg[0];
    
    /*
     * Get preamble first
     */
    msg->preamble = cpu_to_le16 (*pword);

    /*
     * And check it
     */
    if ( msg->preamble != MSG_PREAMBLE ) 
    {
        retcode = FALSE;
        eu_err ("eu_decode_msg: Invalid preamble in message\n");
        goto byebye;
    }

    
    /*
     * Get Function type
     */
    pword ++;
    
    tmp = cpu_to_le16 (*pword);

    msg->type = ( tmp & MP_FUNCTION_TYPE_MASK ) >> MP_FUNCTION_TYPE_LSB;

    /*
     * And check it ...
     */
    switch ( msg->type ) 
    {
        case MP_FUNCTION_TYPE_MEMACCESS:
        case MP_FUNCTION_TYPE_FLASHACC:
        case MP_FUNCTION_TYPE_ADSLDIRECTIVE:
        case MP_FUNCTION_TYPE_MSGDECERR:
        case MP_FUNCTION_TYPE_MSGACCERR:
        case MP_FUNCTION_TYPE_FLASHACCERR:
            /*
             * OK : legal types
             */
            break;
            
        default:
            /*
             * Illegal function type
             */
            retcode = FALSE;
            eu_err ("eu_decode_msg: Illegal function type: 0x%x\n",msg->type);
            goto byebye;
            break;
    }

    /*
     * Get function subtype
     */
    
    tmp = cpu_to_le16 ( *pword );

    msg->subtype = ( tmp & MP_FUNCTION_SUBTYPE_MASK ) >> MP_FUNCTION_SUBTYPE_LSB;

    /*
     * And check it
     */
    switch ( msg->type )
    {
        case MP_FUNCTION_TYPE_MSGACCERR:
            eu_warn ("eu_decode_msg: Received a message 0x%x (subtype 0x%x)",
                     msg->type,msg->subtype);
            break;
            
        case MP_FUNCTION_TYPE_MEMACCESS:
            if ( ( msg->subtype != SUBTYPE_MEMACCESS_REPLYREAD ) &&
                 ( msg->subtype != SUBTYPE_MEMACCESS_REPLYWRITE ) ) 
            {
                retcode = FALSE;
                eu_err ("eu_decode_msg: Illegal function subtype: 0x%x (type=MEMACCESS)\n",msg->subtype);
                goto byebye;
            }
            break;
            
        case MP_FUNCTION_TYPE_FLASHACC:
            switch ( msg->subtype ) 
            {
                case SUBTYPE_FLASHACC_ENKERNELREBOOTREPLY:
			    case SUBTYPE_FLASHACC_ENMODEMREBOOTREPLY:
			    case SUBTYPE_FLASHACC_DECOMPMODEMAPPLREPLY:
			    case SUBTYPE_FLASHACC_STREAMEDDATAWRREPLY:
			    case SUBTYPE_FLASHACC_STREAMEDDATAWRBLOCKREPLY:
			    case SUBTYPE_FLASHACC_READREPLY:
                    /*
                     * OK : legal subtypes
                     */
                    break;
                    
			    default:
                    retcode = FALSE;
                    eu_err ("eu_decode_msg: Illegal function subtype: 0x%x (type=FLASHACC)\n",msg->subtype);
                    goto byebye;
                    break;
            }
            break;

        case MP_FUNCTION_TYPE_FLASHACCERR:
            if ( ( msg->subtype != SUBTYPE_FLASHACCERR_DECOMPRESSERR ) &&
                 ( msg->subtype != SUBTYPE_FLASHACCERR_READCOMPERR ) ) 
            {
                retcode = FALSE;
                eu_err ("eu_decode_msg: Illegal function subtype: 0x%x (type=FLASHACCERR)\n",msg->subtype);
                goto byebye;
            }
            break;

        case MP_FUNCTION_TYPE_ADSLDIRECTIVE:
            if ( ( msg->subtype == SUBTYPE_ADSLDIRECTIVE_KERNELREADY ) ||
                 ( msg->subtype == SUBTYPE_ADSLDIRECTIVE_MODEMREADY ) ||
                 ( msg->subtype == SUBTYPE_ADSLDIRECTIVE_MODEMCRCERROR ) )
            {
                /*
                 * OK : legal subtypes
                 */                
                break;
            }
            
            retcode = FALSE;
            eu_err ("eu_decode_msg: Illegal function subtype: 0x%x (type=ADSLDIRECTIVE)\n",msg->subtype);
            goto byebye;
            break;
            
        default:
            retcode = FALSE;
            eu_err ("eu_decode_msg: Illegal function type: 0x%x\n",msg->type);
            goto byebye;
            break;
    }
    
    /*
     * Get Function Sender ID
     */
    tmp = cpu_to_le16 ( *pword );

    msg->sender_id = ( tmp & MP_FUNCTION_SENDERID_MASK ) >> MP_FUNCTION_SENDERID_LSB;

    if ( msg->sender_id != SUBTYPE_ID_ADSL ) 
    {
        retcode = FALSE;
        eu_err ("eu_decode_msg: Illegal sender id: 0x%x\n",msg->sender_id);
        goto byebye;
    }
    
    /*
     * Get receiver ID
     */
    tmp = cpu_to_le16 ( *pword );

    msg->receiver_id = ( tmp & MP_FUNCTION_RECEIVER_MASK ) >> MP_FUNCTION_RECEIVER_LSB;

    if ( msg->receiver_id != SUBTYPE_ID_ME ) 
    {
        retcode = FALSE;
        eu_err ("eu_decode_msg: Illegal receiver id: 0x%x\n",msg->receiver_id);
        goto byebye;
    }

    if ( msg->type == MP_FUNCTION_TYPE_ADSLDIRECTIVE ) 
    {
        msg->index = 0;
        msg->offset_addr = 0;
        msg->data = 0xDEADBEEF;
        retcode = TRUE;
        goto byebye;
    }

    /*
     * Get index
     */
    pword ++;

    msg->index = cpu_to_le16 ( *pword );

    /*
     * Get Symbolic address
     */
    pword ++;
    
    tmp = cpu_to_le16 ( *pword ); /* Addr High */
    msg->saddr[0] = (unsigned char) ( (unsigned short) ( tmp &0xFF00) >> 8);
    msg->saddr[1] = (unsigned char) ( (unsigned short) ( tmp &0x00FF));
    
    pword ++;
    tmp = cpu_to_le16 ( *pword ); /* Addr Low */
    msg->saddr[2] = (unsigned char) ( (unsigned short) ( tmp &0xFF00) >> 8);
    msg->saddr[3] = (unsigned char) ( (unsigned short) ( tmp &0x00FF));

    /*
     * Get Offset
     */
    pword++;
    msg->offset_addr = *pword;

    /*
     * And data
     */
    pword ++;
    tmp = cpu_to_le16 ( *pword ); /* Addr High */

    pword++;                    /* Addr Low */
    msg->data = tmp * 0x10000 + cpu_to_le16 ( *pword );
    
    
  byebye:
    return (retcode);
    
}


/**
 * eu_send_msg  -  Send given message ( message subsystem must have been initialized)
 *
 */
void eu_send_msg ( eu_instance_t *ins, uint16_t *packet ) 
{
    eu_enters (DBG_MSG);
    
    
    /*
     * if no message pending, its safe to send another one
     */
    if ( ins->AdiModemSm.OutboundPending == FALSE )
    {
        /*
         * Set the flag because the send is pending
         */
        ins->AdiModemSm.OutboundPending = TRUE;
    
        eu_write_dsp_msg ( ins, DSP_MP_TX_START, (uint16_t *) packet );
    
    }
    else
    {
        eu_dbg (DBG_MSG,"TxPending = TRUE, cannot send new message!\n");
        
    }
    eu_leaves (DBG_MSG);
}



/**
 * eu_msg_initialize - Initialize Message subsystem. Pre-encodes all necessary
 *                     messages, and store them in a global structure.
 *
 */
void eu_msg_initialize ( eu_instance_t *ins, const eu_options_t opt )
{
    AdiMSM *pSm = &(ins->AdiModemSm);
    
    
    eu_enters (DBG_MSG);
    
    
    eu_get_cgf_values ( ins, opt );

#if USE_CMVS
    if ((pSm->LineType == 10) &&
        (ins->pDriverCMVs[0].data!=0xF9F9F9F9)) /*Read from CMV text file*/
    {
        int i = 0;
        

        pSm->MsgSeq_Retrainer.MsgCount = 0;

        while (TRUE)
        {
            if (ins->pDriverCMVs[i].data == 0xF9F9F9F9)
            {
                break;
            }
            ADDMSG_FROM_TEXTFILE( pSm->MsgSeq_Retrainer, ins->pDriverCMVs[i]);

            i++;
        }
    } 
    else 
    {
#endif /*  USE_CMVS */
        
        /*
         * Read from configuration file
         * Get our Retrainer message sequence initialized
         */
        pSm->MsgSeq_Retrainer.MsgCount = 0;
        ADDMSG(pSm->MsgSeq_Retrainer, MsgCNTL0_RSVD); /*CNTL0 1 - to have enough time
                                                       * sending retrain CMVs*/
        
        ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgINFO8a,pSm->INFO8);
        
        ADDMSG(pSm->MsgSeq_Retrainer, MsgINFO10);
        
        if (pSm->OPTN0 != 0xF9F8F7F6)
        {
            ADDMSG_FROM_CONFIG(pSm->MsgSeq_Retrainer, MsgOPTN0, pSm->OPTN0);
        }
        
        if (pSm->OPTN2 != 0xF9F8F7F6)
        {
            ADDMSG_FROM_CONFIG(pSm->MsgSeq_Retrainer, MsgOPTN2, pSm->OPTN2);
        }
    
        if (pSm->OPTN4 != 0xF9F8F7F6)
        {
            ADDMSG_FROM_CONFIG(pSm->MsgSeq_Retrainer, MsgOPTN4, pSm->OPTN4);
        }
    
        if (pSm->OPTN7 != 0xF9F8F7F6)
        {
            ADDMSG_FROM_CONFIG(pSm->MsgSeq_Retrainer, MsgOPTN7, pSm->OPTN7);
        }
    
        /*
         * The ADI code checks the ISDN var AND the OPTN3 var, but if OPTN3
         * is only set for ISDN - there's no need for the ISDN var
         */
        if (pSm->OPTN3 != 0xF9F8F7F6)
        {
            ADDMSG_FROM_CONFIG(pSm->MsgSeq_Retrainer, MsgOPTN3, pSm->OPTN3);
        }
    
        if (pSm->OPTN5 != 0xF9F8F7F6)
        {
            ADDMSG_FROM_CONFIG(pSm->MsgSeq_Retrainer, MsgOPTN5, pSm->OPTN5);
        }
     
        if (pSm->OPTN6 != 0xF9F8F7F6)
        {
            ADDMSG_FROM_CONFIG(pSm->MsgSeq_Retrainer, MsgOPTN6, pSm->OPTN6);
        }
    
        if (pSm->OPTN15 != 0xF9F8F7F6)
        {
            ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgOPTN15,pSm->OPTN15);
        }
    
        ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgADPT2, pSm->ADPT2);
        ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgINFO9, pSm->INFO9);
        ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgMASK8, pSm->MASK8);
        ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgMASK9, pSm->MASK9);
        ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgPSDM0, pSm->PSDM0);
        
        if (pSm->LineType == 0)
        {
            /*
             * G.Lite
             */
            ADDMSG_IFVAR_NOTZERO(pSm->MsgSeq_Retrainer, MsgPFCL1, pSm->PFCL1);
        }
        
    
        /*
         * To tell modem it can go ahead to R_ACT_REQUEST state
         */ 
        ADDMSG(pSm->MsgSeq_Retrainer, MsgCNTL0_INIT);
#if USE_CMVS
    }
#endif /* USE_CMVS */
    
    /*
     * Get our SoftReset message sequence initialized
     */
    pSm->MsgSeq_SoftReset.MsgCount = 0;
    ADDMSG( pSm->MsgSeq_SoftReset, MsgCNTL0_RSET);
    
    /*
     * Get our Stat message sequence initialized
     */
    pSm->MsgSeq_Stat.MsgCount = 0;
    ADDMSG( pSm->MsgSeq_Stat, MsgSTAT0b);
    
    /*
     * Get our OpStat message sequence initialized
     */
    pSm->MsgSeq_OpStat.MsgCount = 0;
    ADDMSG( pSm->MsgSeq_OpStat, MsgSTAT0a);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG2);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG22);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG23);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG25);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG51);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG52);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG53);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG54);
    ADDMSG( pSm->MsgSeq_OpStat, MsgRATE0);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG3);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG47);
    ADDMSG( pSm->MsgSeq_OpStat, MsgDIAG49);
    ADDMSG( pSm->MsgSeq_OpStat, MsgINFOA);
    ADDMSG( pSm->MsgSeq_OpStat, MsgPSDM1);
    ADDMSG( pSm->MsgSeq_OpStat, MsgINFO8b);
    ADDMSG( pSm->MsgSeq_OpStat, MsgINFO14);
    ADDMSG( pSm->MsgSeq_OpStat, MsgSTAT0b);    
    
    /*
     * Get our ModemEna message sequence initialized
     */
    pSm->MsgSeq_ModemEna.MsgCount = 0;
    ADDMSG( pSm->MsgSeq_ModemEna, MsgMODEMENA);
    
    /*
     * Set Enable Fast Retrain options
     */
    pSm->MsgSeq_EnaFR.MsgCount = 0;
    ADDMSG( pSm->MsgSeq_EnaFR, MsgOPTN0EnaFR);
    
    /*
     * Init our message pending flags
     */
    pSm->InboundAsyncPending = FALSE;
    pSm->InboundSyncPending  = FALSE;
    pSm->OutboundPending     = FALSE;
    
    MsgInitDone = TRUE;
    

    
    eu_leaves (DBG_MSG);
    
}

/* ----------------------------- Private Functions ------------------------------ */


/**
 * eu_get_cgf_values - Prepares options for state machine.
 *
 */
static void eu_get_cgf_values ( eu_instance_t *ins, const eu_options_t opt )
{
    AdiMSM *pSm = &(ins->AdiModemSm);

    eu_enters (DBG_MSG);
    

    /*
     * Initialize defaults
     */
    pSm->ADPT0 = 0;
    pSm->ADPT1 = 0;
    pSm->ADPT2 = 0;
    pSm->INFO9 = 0;
    pSm->CNTL0 = 0;
    pSm->OPTN0 = 0;
    pSm->OPTN2 = 0;
    pSm->OPTN4 = 0;
    pSm->OPTN7 = 0;
    pSm->OPTN3 = 0;
    pSm->OPTN5 = 0;
    pSm->OPTN6 = 0;
    pSm->PFCL1 = 0;
    pSm->MASK8 = 0;
    pSm->MASK9 = 0;
    pSm->INFO8 = 0;
    pSm->PSDM0 = 0;
    pSm->FLAG0 = 0;

    /*
     * Get the option values from the hardware struct
     */
    pSm->OPTN0 = opt[CFG_OPT_0].value;
    pSm->OPTN2 = opt[CFG_OPT_2].value;
    pSm->OPTN3 = opt[CFG_OPT_3].value;
    pSm->OPTN4 = opt[CFG_OPT_4].value;
    pSm->OPTN5 = opt[CFG_OPT_5].value;
    pSm->OPTN6 = opt[CFG_OPT_6].value;
    pSm->OPTN7 = opt[CFG_OPT_7].value;
    pSm->OPTN15 = opt[CFG_OPT_15].value;
    pSm->LineType = opt[CFG_LINE].value;
     
    eu_leaves (DBG_MSG);
    
}

/**
 * eu_build_msg - Build a message to architecture indep form.
 *
 */
static void eu_build_msg ( eu_cmv_msg_t *dst, eu_cmv_msg_t *src ) 
{
    uint8_t tmp;
    
    dst->preamble = cpu_to_le16 ( src->preamble );
    dst->function = cpu_to_le16 ( src->function );
    dst->idx      = cpu_to_le16 ( src->idx );
    dst->offset   = cpu_to_le16 ( src->offset );

    /*
     * Usefull when src == dst
     */
    tmp = src->saddr_hihi;
    
    dst->saddr_hihi = src->saddr_hilo;
    dst->saddr_hilo = tmp;

    tmp = src->saddr_lohi;
    dst->saddr_lohi = src->saddr_lolo;
    dst->saddr_lolo = tmp;

    dst->data = ___swahw32 (cpu_to_le32 (src->data));

}



/**
 * eu_write_dsp_msg - Write to DSP.
 *
 */
static void eu_write_dsp_msg (
                              eu_instance_t *ins,
                              uint16_t       addr,
                              uint16_t      *data
                             )
{
    struct urb *urb;
    devrequest *pdr;
    uint8_t *xfer_buff = NULL;

    xfer_buff = GET_KBUFFER ( ( CMV_DATA_WORDS*2 ) + sizeof(devrequest) );

    if ( !xfer_buff )
    {
        eu_err ("eu_write_dsp_msg: cannot allocate %#lx bytes\n",
                (unsigned long)(CMV_DATA_WORDS*2) + sizeof(devrequest) );
        goto byebye;
    }

    memcpy ( xfer_buff, data, CMV_DATA_WORDS*2 );
    pdr = (devrequest *) ( xfer_buff + CMV_DATA_WORDS*2 );

    /*
     * Get an URB and prepare it for submission
     */
    urb = USB_ALLOC_URB ( 0 , GFP_ATOMIC );

    FILL_USB_CTRL_REQUEST ( pdr,
                            USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                            CDC_REQ_SEND_ENCAPSULATED_CMD,
                            CMD_SET_BLOCK, (addr | 0x4000), CMV_DATA_WORDS*2 );

    usb_fill_control_urb ( urb,
                           ins->usbdev,
                           usb_sndctrlpipe(ins->usbdev,0),
                           (uint8_t *) pdr,
                           xfer_buff,
                           CMV_DATA_WORDS * 2,
                           ctrl_urb_completion,
                           NULL );
    
    queue_ctrl_urb ( ins, urb );

  byebye:
    return ;
}


