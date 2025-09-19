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
 */
#include "ns3/log.h"
#include "ns3/uinteger.h"

#include "xgpon-onu-us-scheduler.h"



NS_LOG_COMPONENT_DEFINE ("XgponOnuUsScheduler");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (XgponOnuUsScheduler);


TypeId 
XgponOnuUsScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::XgponOnuUsScheduler")
    .SetParent<Object> ()
    .AddAttribute("BaseGrantSize",
              "Base Grant size of XGPON/XGSPON (Unit: blocks of word for XGPON or 4-word for XGSPON)",
              UintegerValue(BASE_GRANT_SIZE_XGPON),
              MakeUintegerAccessor(&XgponOnuUsScheduler::m_baseGrantSize),
              MakeUintegerChecker<uint8_t>())
  ;
  return tid;
}
TypeId 
XgponOnuUsScheduler::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}



XgponOnuUsScheduler::XgponOnuUsScheduler ():m_tcontOnu(0), m_nullConn(0), m_baseGrantSize(BASE_GRANT_SIZE_XGPON)
{
}
XgponOnuUsScheduler::~XgponOnuUsScheduler ()
{
}







const Ptr<XgponTcontOnu>& 
XgponOnuUsScheduler::GetTcontOnu ( ) const
{
  NS_ASSERT_MSG((m_tcontOnu!=nullptr), "TCONT-ONU has not been set yet.");
  return m_tcontOnu;
}
void 
XgponOnuUsScheduler::SetTcontOnu (const Ptr<XgponTcontOnu>& tcont)
{
  m_tcontOnu = tcont;
}
  

}; // namespace ns3

