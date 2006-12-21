/* BEGIN_COPYRIGHT                                                        */
/*                                                                        */
/* Open Diameter: Open-source software for the Diameter and               */
/*                Diameter related protocols                              */
/*                                                                        */
/* Copyright (C) 2002-2004 Open Diameter Project                          */
/*                                                                        */
/* This library is free software; you can redistribute it and/or modify   */
/* it under the terms of the GNU Lesser General Public License as         */
/* publishvalied by the Free Software Foundation; either version 2.1 of the   */
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

/* $Id: */
/* 
   cc_server_test.cxx
   Server test program for Diameter CC Application 
*/

#include <iostream>
#include <ace/Log_Msg.h>
#include <ace/OS.h>
#include "diameter_api.h"
#include "diameter_cc_parser.h"
#include "diameter_cc_server_session.h"
#include "diameter_cc_account.h"
#include "diameter_cc_application.h"


class CCServerSession : public DiameterCCServerSession
{
 public:
  CCServerSession(DiameterCCApplication& ccApp,
                  diameter_unsigned32_t appId=CCApplicationId) 
    : DiameterCCServerSession(ccApp, appId)
  {
    this->Start();
  }

  void Start() throw(AAA_Error)
  { 
    DiameterCCServerSession::Start(); 
  }

  /// This virtual function is called when a CC server session is
  /// aborted due to enqueue failure of a job or an event inside
  /// Diameter CC server state machine.
  void Abort()
  {
    std::cout << "Diameter CC server session aborted." << std::endl;
    DiameterCCServerSession::Stop();
  }

  virtual bool InitialRequest();

  virtual bool InitialAnswer();

  virtual bool TerminationRequest();

  virtual bool CheckBalance ();

private:

};


bool
CCServerSession::InitialRequest()
{
  std::vector<subscriptionId_t>& vec = ccrData.SubscriptionId();
  Account(diameterCCApplication.getAccount(vec[0]));

  DiameterCCAccount& account = Account();
  requestedServiceUnit_t& balanceUnit = account.BalanceUnits();
  requestedServiceUnit_t& requestedServiceUnit = ccrData.RequestedServiceUnit();
  AAA_LOG((LM_DEBUG, "(%P|%t) \tRequested %d Service Units.\n",
           requestedServiceUnit.CCMoney().UnitValue().ValueDigits()));
  const requestedServiceUnit_t& units = balanceUnit - requestedServiceUnit;
  if (units > 0)
    {
      account.ReservedUnits(requestedServiceUnit);
      account.BalanceUnits(units);
      
      AAA_LOG((LM_DEBUG, "(%P|%t) \tAccount has %d Balance Units.\n",
               account.BalanceUnits().CCMoney().UnitValue().ValueDigits()));
      AAA_LOG((LM_DEBUG, "(%P|%t) \tAccount has %d Reserved Units.\n",
               account.ReservedUnits().CCMoney().UnitValue().ValueDigits()));

      grantedServiceUnit_t grantedServiceUnit = ccrData.RequestedServiceUnit();
      ccaData.GrantedServiceUnit = grantedServiceUnit;

      return true;
    }
  else
    {
      return false;
    }
}

bool
CCServerSession::InitialAnswer()
{
  DiameterCCAccount& account = Account();
  grantedServiceUnit_t grantedServiceUnit = account.ReservedUnits();  
  ccaData.GrantedServiceUnit = grantedServiceUnit;

  AAA_LOG((LM_DEBUG, "(%P|%t) \tGranted %d Service Units.\n",
           grantedServiceUnit.CCMoney().UnitValue().ValueDigits()));
  
  return true;
}

bool
CCServerSession::TerminationRequest()
{  
  DiameterCCAccount& account = Account();
  const requestedServiceUnit_t& balanceUnits = account.BalanceUnits();
  const requestedServiceUnit_t& reservedUnits = account.ReservedUnits();
  const requestedServiceUnit_t& usedUnits = ccrData.UsedServiceUnit()[0];

  const requestedServiceUnit_t& units =   balanceUnits + (reservedUnits - usedUnits);
  
  account.BalanceUnits(units);

  unitValue_t unitValue (0, 0);
  ccMoney_t ccMoney(unitValue,840);
  requestedServiceUnit_t clearReserved(0,ccMoney);
  account.ReservedUnits(clearReserved);
  
  AAA_LOG((LM_DEBUG, "(%P|%t) \tSubscriber Used %d Units.\n"
           ,ccrData.UsedServiceUnit()[0].CCMoney().UnitValue().ValueDigits()));
  AAA_LOG((LM_DEBUG, "(%P|%t) \tAccount has %d Balance Units.\n"
           ,account.BalanceUnits().CCMoney().UnitValue().ValueDigits()));
  AAA_LOG((LM_DEBUG, "(%P|%t) \tAccount has  %d Reserved Units.\n"
           ,account.ReservedUnits().CCMoney().UnitValue().ValueDigits()));

  diameterCCApplication.setAccount(ccrData.SubscriptionId()[0], account);
  return true;
}

bool
CCServerSession::CheckBalance()
{
  AAA_LOG((LM_DEBUG, "(%P|%t) \tCheck Balance.\n"));

  std::vector<subscriptionId_t>& vec = ccrData.SubscriptionId();
  Account(diameterCCApplication.getAccount(vec[0]));

  DiameterCCAccount& account = Account();
  requestedServiceUnit_t& balanceUnits = account.BalanceUnits();

  AAA_LOG((LM_DEBUG, "(%P|%t) \tAccount has %d Balance Units.\n"
           ,account.BalanceUnits().CCMoney().UnitValue().ValueDigits()));
  return true;
}

class CCServerSessionClassFactory : 
  public AAAServerSessionFactory
{
    public:
  CCServerSessionClassFactory(DiameterCCApplication &c,
                              diameter_unsigned32_t appId) : 
    AAAServerSessionFactory(c.GetTask(), appId),
    m_Core(c)
    
  {
  }
  
  DiameterSessionIO *CreateInstance() 
  {        
    CCServerSession *s = new CCServerSession
      (m_Core, GetApplicationId());
    return s->IO();
  }
  
private:
  DiameterCCApplication &m_Core;
};

typedef CCServerSessionClassFactory CCServerFactory;

class CCInitializer
{
 public:
  CCInitializer(AAA_Task &t, DiameterCCApplication &diameterCCApp) 
    : task(t), diameterCCApplication(diameterCCApp)
  {
    Start();
  }

  ~CCInitializer() 
  {
    Stop();
  }

 private:

  void Start()
  {
    InitTask();
    InitCCApplication();
    ccServerFactory = std::auto_ptr<CCServerFactory>
      (new CCServerFactory(diameterCCApplication, CCApplicationId));
    diameterCCApplication.RegisterServerSessionFactory(ccServerFactory.get());
  }

  void Stop() {}

  void InitTask()
  {
     try {
        task.Start(10);
     }
     catch (...) {
        ACE_ERROR((LM_ERROR, "(%P|%t) Server: Cannot start task\n"));
        exit(1);
     }
  }
  void InitCCApplication()
  {
    AAA_LOG((LM_DEBUG, "(%P|%t) Application starting\n"));
    if (diameterCCApplication.Open("config/server.local.xml",
                   task) != AAA_ERR_SUCCESS)
      {
        AAA_LOG((LM_ERROR, "(%P|%t) Can't open configuraiton file."));
        exit(1);
      }
    

    subscriptionId_t subscriptionId(0,"1"); //END_USER_E164
    unitValue_t unitValue (100, 0);
    ccMoney_t ccMoney(unitValue,840);
    AAA_LOG((LM_DEBUG, "(%P|%t) Account has %d Balance Units.\n",unitValue.ValueDigits() ));

    requestedServiceUnit_t balanceunits(0,ccMoney);
    diameterCCApplication.addAccount(subscriptionId, balanceunits);
  }

  AAA_Task &task;
  DiameterCCApplication &diameterCCApplication;
  std::auto_ptr<CCServerFactory> ccServerFactory;
};

int
main(int argc, char *argv[])
{
  AAA_Task ccTask;
  DiameterCCApplication diameterCCApplication;
  CCInitializer initializer(ccTask, diameterCCApplication);

  while (1) 
      ACE_OS::sleep(1);
  return 0;
}
