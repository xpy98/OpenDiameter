/* BEGIN_COPYRIGHT                                                        */
/*                                                                        */
/* Open Diameter: Open-source software for the Diameter and               */
/*                Diameter related protocols                              */
/*                                                                        */
/* Copyright (C) 2002-2007 Open Diameter Project                          */
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

/* Author   : Victor I. Fajardo
 * Synopsis : Base class for accouting support
 */

#include "aaa_msg_to_xml.h"
#include "aaa_log_facility.h"
#include "diameter_api.h"


/*!
 *  <Message>
 *     <version>value</version>
 *     <flags request="value" proxiable="value" error="value" retrans="value"></flags>
 *     <code>value</code>
 *     <appId>value</appId>
 *     <HopId>value</HopId>
 *     <EndId>value</EndId>
 *     <avp>
 *        <"avpname">value</avp>
 *          .
 *          .
 *        <"avpname">
 *           <"avpname">value</"avpname">
 *           <"avpname">value</"avpname">
 *               .
 *               .
 *           <"avpname">
 *              <"avpname">value</"avpname">
 *                 .
 *                 .
 *              </"avpname">
 *        </"avpname">
 *     </avp>
 *  </Message>
 */
AAAReturnCode DiameterAccountingXMLRecTransformer::Convert(DiameterMsg *msg)
{
   AAAXmlWriter writer;

   std::string output;
   writer.writeToString(msg, output);

   if (output.length()) {
      record = reinterpret_cast<void*>(ACE_OS::strdup(output.c_str()));
      record_size = ACE_OS::strlen(output.c_str());
   }
   else {
      record = NULL;
      record_size = 0;
   }

   return (AAA_ERR_SUCCESS);
}

AAAReturnCode DiameterAccountingXMLRecTransformer::OutputRecord(DiameterMsg *originalMessage)
{
   AAA_LOG((LM_DEBUG, "(%P|%t) Server: Default output record handler\n"));

   if (record) {

      AAA_LOG((LM_DEBUG, "(%P|%t) Server: Resetting record holder\n"));

      ACE_OS::free(record);
      record = NULL;
      record_size = 0;
   }
   return (AAA_ERR_SUCCESS);
}

