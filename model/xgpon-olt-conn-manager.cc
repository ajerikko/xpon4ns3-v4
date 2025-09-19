/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 University College Cork (UCC), Ireland
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Xiuchao Wu <xw2@cs.ucc.ie>
 * Author: Jerome Arokkiam <jerome.arokkia@bt.com>
 */

#include "ns3/log.h"

#include "xgpon-olt-conn-manager.h"



NS_LOG_COMPONENT_DEFINE ("XgponOltConnManager");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (XgponOltConnManager);

TypeId 
XgponOltConnManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::XgponOltConnManager")
    .SetParent<XgponOltEngine> ()
  ;
  return tid;
}
TypeId 
XgponOltConnManager::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}




XgponOltConnManager::XgponOltConnManager (): m_dsConnsPortIndex(65536,(Ptr<XgponConnectionSender>)0),
  m_broadcastConns(0), 
  m_onus(1024,(Ptr<XgponOltConnPerOnu>)0), 
  m_tconts(16384,(Ptr<XgponTcontOlt>)0), 
  m_tcontsType(4,(XgponQosParameters::XgponTcontType)0) //jerome, Apr 9; ja:update:ns-3.35, 4 instead of 16384
{
}
XgponOltConnManager::~XgponOltConnManager ()
{
}



void 
XgponOltConnManager::AddOneOnu4Conns (const Ptr<XgponOltConnPerOnu>& onu4Conns)
{
  NS_LOG_FUNCTION(this);

  int i = onu4Conns->GetOnuId ();
  NS_ASSERT_MSG((i<1021), "ONU-ID is too large (unlawful)!!!"); //ja:update:xgspon maximum onu-id that can be assigned as per G9807.1Amd2(10/2020) is 1020. 1021 is Reserved; 1022 and 1023 are for Broadcast/unasssigned. This is a subset of G987.3 XGPON amendments; hence the lower is used for dual (xgpon/xgspon) compatibility

  m_onus[i] = onu4Conns;
}
const Ptr<XgponOltConnPerOnu>& 
XgponOltConnManager::GetOneOnu4ConnsById (uint16_t onuId) const 
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT_MSG((onuId<1021), "Onu-ID is too large (unlawful)!!!"); //ja:update:xgspon see comment above

  return m_onus[onuId];
}





void 
XgponOltConnManager::AddOneUsTcont (const Ptr<XgponTcontOlt>& tcont, uint16_t onuId)
{
  NS_LOG_FUNCTION(this);

  NS_ASSERT_MSG((onuId<1023), "ONU-ID is too large (unlawful)!!!");

  const Ptr<XgponOltConnPerOnu>& onu  = GetOneOnu4ConnsById (onuId);
  if(onu != nullptr)
  {
    NS_ASSERT_MSG((tcont->GetAllocId()<16384), "Alloc-ID is too large (unlawful)!!!");

    onu->AddOneUsTcont(tcont);
    m_tconts[tcont->GetAllocId()] = tcont;
  }
}



void 
XgponOltConnManager::AddOneUsConn (const Ptr<XgponConnectionReceiver>& conn, uint16_t allocId)
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT_MSG((allocId<16384), "Alloc-ID is too large (unlawful)!!!");

  const Ptr<XgponTcontOlt>& tcont = GetTcontById (allocId);
  //jerome, Apr 9
  Ptr<XgponQosParameters> qosParameters = tcont->GetQosParameters();
  m_tcontsType[allocId] = qosParameters->GetTcontType();

  if(tcont != nullptr) tcont->AddOneConnection(conn);
}









uint32_t
XgponOltConnManager::GetNumberOfOnus ()
{
  NS_LOG_FUNCTION(this);

  uint32_t numOnus=0;
  
  for(uint32_t i=0;i<m_onus.size();i++)
    if(m_onus[i]!=nullptr)
      numOnus++;

  return numOnus;
}


uint32_t
XgponOltConnManager::GetNumberOfTconts ()
{
  NS_LOG_FUNCTION(this);

  uint32_t numTconts=0;

  for(uint32_t i=0; i<m_onus.size(); i++)
  {
    if(m_onus[i]!=nullptr) numTconts += m_onus[i]->GetNumberOfTconts();   
  }

  return numTconts;
}














}; // namespace ns3

