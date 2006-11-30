/* BEGIN_COPYRIGHT                                                        */
/*                                                                        */
/* Open Diameter: Open-source software for the Diameter and               */
/*                Diameter related protocols                              */
/*                                                                        */
/* Copyright (C) 2002-2004 Open Diameter Project                          */
/*                                                                        */
/* This library is free software; you can redistribute it and/or modify   */
/* it under the terms of the GNU Lesser General Public License as         */
/* published by the Free Software Foundation; either version 2.1 of the   */
/* License, or (at your option) any later version.                        */
/*                                                                        */
/* This library is distributed in the hope that it will be useful,        */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      */
/* Lesser General Public License for more details.                        */
/*                                                                        */
/* You should have received a copy of the GNU Lesser General Public       */
/* License along with this library; if not, write to the Free Software    */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307    */
/* USA.                                                                   */
/*                                                                        */
/* In addition, when you copy and redistribute some or the entire part of */
/* the source code of this software with or without modification, you     */
/* MUST include this copyright notice in each copy.                       */
/*                                                                        */
/* If you make any changes that are appeared to be useful, please send    */
/* sources that include the changed part to                               */
/* diameter-developers@lists.sourceforge.net so that we can reflect your  */
/* changes to one unified version of this software.                       */
/*                                                                        */
/* END_COPYRIGHT                                                          */

#include "aaa_transport_collector.h"

DiameterMsgCollector::DiameterMsgCollector() :
    m_Buffer(NULL),
    m_Offset(0),
    m_BufSize(0),
    m_MsgLength(0),
    m_Handler(NULL)
{
   m_Buffer = (char*)ACE_OS::malloc(DIAMETER_HEADER_SIZE +
                                    MAX_MSG_LENGTH);
   if (m_Buffer != NULL) {
      m_BufSize = DIAMETER_HEADER_SIZE + MAX_MSG_LENGTH;
      ACE_OS::memset(m_Buffer, 0, m_BufSize);
      m_PersistentError.Reset(0, 0,
              (m_BufSize * MAX_MSG_BLOCK)/sizeof(ACE_UINT32));
   }
}

DiameterMsgCollector::~DiameterMsgCollector()
{
   if (m_Buffer) {
      ACE_OS::free(m_Buffer);
   }
}

void DiameterMsgCollector::Message(void *data, size_t length,
                                   const Diameter_IO_Base *io)
{
   if (m_Buffer == NULL || m_Handler == NULL) {
      return;
   }

   int r_bytes = 0;
   bool bHasHeaderError = false;
   std::string eDesc;

   while (length > 0) {
      r_bytes = (length > size_t(m_BufSize - m_Offset)) ?
                m_BufSize - m_Offset : length;
      ACE_OS::memcpy(m_Buffer + m_Offset, data, r_bytes);

      length -= r_bytes;
      data = (char*)data + r_bytes;
      m_Offset += (r_bytes > 0) ? r_bytes : 0;

      while (m_Offset > DIAMETER_HEADER_SIZE) {

         DiameterMsgHeader hdr;

         if (m_MsgLength == 0) {

            AAAMessageBlock *aBuffer = AAAMessageBlock::Acquire
                (m_Buffer, DIAMETER_HEADER_SIZE);

            DiameterMsgHeaderParser hp;
            hp.setRawData(aBuffer);
            hp.setAppData(&hdr);
            hp.setDictData(DIAMETER_PARSE_STRICT);

            try {
               hp.parseRawToApp();
               bHasHeaderError = false;
            }
            catch (DiameterErrorCode &st) {
               int eCode;
               AAA_PARSE_ERROR_TYPE eType;
               st.get(eType, eCode, eDesc);

               DiameterRangedValue lengthRange
                  (hdr.length, DIAMETER_HEADER_SIZE, m_BufSize * MAX_MSG_BLOCK);

               if ((eCode == AAA_COMMAND_UNSUPPORTED) &&
                   ! lengthRange.InRange()) {
                   // cannot trust reported header length

                   aBuffer->Release();
                   if (++m_PersistentError) {
                       eDesc = "To many persistent errors in data";
                       m_Handler->Error
                          (DiameterMsgCollectorHandler::CORRUPTED_BYTE_STREAM, eDesc);
                       return;
                   }
                   m_Offset -= sizeof(ACE_UINT32);
                   ACE_OS::memcpy(m_Buffer, m_Buffer + sizeof(ACE_UINT32),
                                 m_Offset);
                   continue;
               }
               else if (eType == AAA_PARSE_ERROR_TYPE_NORMAL) {
                   // can trust reported header length despite error
                   bHasHeaderError = true;
                   std::auto_ptr<DiameterMsg> answerMsg = DiameterErrorMsg::Generate(hdr, eCode);
                   DiameterMsgCollector::Send(answerMsg, (Diameter_IO_Base*)io);
               }
               else {
                   eDesc = "Bug present in the parsing code !!!!";
                   m_Handler->Error
                      (DiameterMsgCollectorHandler::BUG_IN_PARSING_CODE, eDesc);
                   return;
               }
            }
            aBuffer->Release();
            m_MsgLength = hdr.length;
         }

         if (m_MsgLength > m_Offset) {
            if (m_MsgLength > m_BufSize) {
               m_Buffer = (char*)ACE_OS::realloc(m_Buffer,
                                                 m_MsgLength + 1);
               if (m_Buffer) {
                  ACE_OS::memset(m_Buffer + m_BufSize, 0,
                                 m_MsgLength - m_BufSize + 1);
                  m_BufSize = m_MsgLength + 1;
               }
               else {
                  m_BufSize = 0;
                  m_Offset = 0;
                  m_MsgLength = 0;

                  eDesc = "Byte buffer too small but failed to re-alloc";
                  m_Handler->Error
                      (DiameterMsgCollectorHandler::MEMORY_ALLOC_ERROR, eDesc);
                  return; 
               }
            }
            else {
	       break;
            }
         }
         else {

            if (! bHasHeaderError) {

                AAAMessageBlock *aBuffer = NULL;
                std::auto_ptr<DiameterMsg> msg(new DiameterMsg);
                try {
                   if (msg.get() == NULL) {
                      throw (0);
                   }
                   aBuffer = AAAMessageBlock::Acquire
                                (m_Buffer, m_MsgLength);
                   if (aBuffer == NULL) {
                      throw (0);
                   }

                   msg->hdr = hdr;

                   DiameterMsgPayloadParser pp;
                   pp.setRawData(aBuffer);
                   pp.setAppData(&msg->acl);
                   pp.setDictData(msg->hdr.getDictHandle());

                   aBuffer->rd_ptr(aBuffer->base() + DIAMETER_HEADER_SIZE);
                   pp.parseRawToApp();
                   aBuffer->Release();
                }
                catch (DiameterErrorCode &st) {

                   aBuffer->Release();

                   int eCode;
                   AAA_PARSE_ERROR_TYPE eType;
                   st.get(eType, eCode, eDesc);

                   if (eType == AAA_PARSE_ERROR_TYPE_NORMAL) {

                       SendFailedAvp(st, (Diameter_IO_Base*)io);

                       aBuffer->Release();
                       m_Offset -= m_MsgLength;
                       ACE_OS::memcpy(m_Buffer, m_Buffer + m_Offset,
                                      m_MsgLength);

                       eDesc = "Payload error encounted in newly arrived message";
                       m_Handler->Error
                           (DiameterMsgCollectorHandler::PARSING_ERROR, eDesc);
                       continue;
                   }
                   else {
                       eDesc = "Bug present in the parsing code !!!!";
                       m_Handler->Error
                          (DiameterMsgCollectorHandler::BUG_IN_PARSING_CODE, eDesc);
                       return;
                   }
                } catch (...) {
                   if ((msg.get() == NULL) || (aBuffer == NULL)) {
                      m_BufSize = 0;
                      m_Offset = 0;
                      m_MsgLength = 0;

                      eDesc = "Byte buffer too small but failed to re-alloc";
                      m_Handler->Error
                          (DiameterMsgCollectorHandler::MEMORY_ALLOC_ERROR, eDesc);
                      return;
                   }
                   if (aBuffer) {
                      aBuffer->Release();
                   }
                }

                m_Handler->Message(msg);

                m_PersistentError.Reset(0, 0,
                    (m_BufSize * MAX_MSG_BLOCK)/sizeof(ACE_UINT32));
            }
            else {
                eDesc = "Header error encounted in newly arrived message";
                m_Handler->Error
                    (DiameterMsgCollectorHandler::PARSING_ERROR, eDesc);
            }

            if (m_MsgLength < m_Offset) {
               ACE_OS::memcpy(m_Buffer, m_Buffer + m_MsgLength,
                              m_Offset - m_MsgLength);
               m_Offset -= m_MsgLength;
            }
            else {
               m_Offset = 0;
            }

            ACE_OS::memset(m_Buffer + m_Offset, 0, m_BufSize - m_Offset);
            m_MsgLength = 0;
         }
      }
   }
}

void DiameterMsgCollector::SendFailedAvp(DiameterErrorCode &st,
                                         Diameter_IO_Base *io)
{
   // AAA_OUT_OF_SPACE
   // AAA_INVALID_AVP_VALUE
   // AAA_AVP_UNSUPPORTED
   // AAA_MISSING_AVP
   // AAA_INVALID_AVP_BITS
   // AAA_AVP_OCCURS_TOO_MANY_TIMES

   // TBD:
   // std::auto_ptr<DiameterMsg> answerMsg = DiameterErrorMsg::Generate(msg, eCode);
   // DiameterMsgCollector::Send(answerMsg, io);
}

int DiameterMsgCollector::Send(std::auto_ptr<DiameterMsg> &msg,
                               Diameter_IO_Base *io)
{
   AAAMessageBlock *aBuffer = NULL;

   for (int blockCnt = 1;
        blockCnt <= DiameterMsgCollector::MAX_MSG_BLOCK;
        blockCnt ++) {

       aBuffer = AAAMessageBlock::Acquire
             (DiameterMsgCollector::MAX_MSG_LENGTH * blockCnt);

       msg->acl.reset();

       DiameterMsgHeaderParser hp;
       hp.setRawData(aBuffer);
       hp.setAppData(&msg->hdr);
       hp.setDictData(DIAMETER_PARSE_STRICT);

       try {
          hp.parseAppToRaw();
       }
       catch (DiameterErrorCode &st) {
          ACE_UNUSED_ARG(st);
          aBuffer->Release();
          return (-1);
       }

       DiameterMsgPayloadParser pp;
       pp.setRawData(aBuffer);
       pp.setAppData(&msg->acl);
       pp.setDictData(msg->hdr.getDictHandle());

       try { 
          pp.parseAppToRaw();
       }
       catch (DiameterErrorCode &st) {
          aBuffer->Release();

          AAA_PARSE_ERROR_TYPE type;
          int code;
          st.get(type, code);
          if ((type == AAA_PARSE_ERROR_TYPE_NORMAL) && (code == AAA_OUT_OF_SPACE)) {
              if (blockCnt < DiameterMsgCollector::MAX_MSG_BLOCK) {
                  msg->acl.reset();
                  continue;
              }
              AAA_LOG((LM_ERROR, "(%P|%t) Not enough block space for transmission\n"));
          }
          return (-1);
      }

      msg->hdr.length = aBuffer->wr_ptr() - aBuffer->base();
      try {
          hp.parseAppToRaw();
      }
      catch (DiameterErrorCode &st) {
          aBuffer->Release();
          return (-1);
      }
      break;
   }

   aBuffer->length(msg->hdr.length);
   return io->Send(aBuffer);
}
