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
 * Author: Jerome Arokkiam <jerome.arokkiam@bt.com>
 */

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include "xgpon-onu-dba-engine.h"
#include "xgpon-onu-net-device.h"



NS_LOG_COMPONENT_DEFINE ("XgponOnuDbaEngine");

namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (XgponOnuDbaEngine);

TypeId 
XgponOnuDbaEngine::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::XgponOnuDbaEngine")
    .SetParent<XgponOnuEngine> ()
    .AddConstructor<XgponOnuDbaEngine> ()
    .AddAttribute("BaseGrantSize",
              "Base Grant size of XGPON/XGSPON (Unit: blocks of word for XGPON or blocks of 4-word for XGSPON)",
              UintegerValue(BASE_GRANT_SIZE_XGPON),
              MakeUintegerAccessor(&XgponOnuDbaEngine::m_baseGrantSize),
              MakeUintegerChecker<uint8_t>())
	;
  return tid;
}
TypeId
XgponOnuDbaEngine::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}



XgponOnuDbaEngine::XgponOnuDbaEngine (): 
	m_baseGrantSize(BASE_GRANT_SIZE_XGPON) //ja:update:xgspon
{
}
XgponOnuDbaEngine::~XgponOnuDbaEngine ()
{
}


Ptr<XgponXgtcDbru> 
XgponOnuDbaEngine::GenerateStatusReport (uint16_t allocId)
{
  NS_LOG_FUNCTION(this);
	//std::cout << "at_time_SR:" << Simulator::Now().GetMicroSeconds() << ",generateSR,allocId," << allocId << std::endl;
  return ((m_device->GetConnManager())->GetTcontById(allocId))->PrepareBufOccupancyReport ( );
}






void 
XgponOnuDbaEngine::ProcessBwMap (const Ptr<XgponXgtcBwmap>& bwmap)
{
  NS_LOG_FUNCTION(this);
  uint64_t nowNano = Simulator::Now().GetNanoSeconds();

  const Ptr<XgponOnuConnManager>& connManager = m_device->GetConnManager();
  const Ptr<XgponPhy>& commonPhy = m_device->GetXgponPhy();
  const Ptr<XgponLinkInfo>& linkInfo = (m_device->GetPloamEngine())->GetLinkInfo ();
	 
  uint16_t bwMapSize=bwmap->GetNumberOfBwAllocation ( );
  for(int i=0; i<bwMapSize; i++)
  {
    const Ptr<XgponXgtcBwAllocation>& bwAlloc=bwmap->GetBwAllocationByIndex(i); 
    uint32_t startTime = bwAlloc->GetStartTime();
    uint16_t allocId = bwAlloc->GetAllocId();

    const Ptr<XgponTcontOnu>& tcontOnu = connManager->GetTcontById(allocId);
    if(tcontOnu!=nullptr)
    {
      tcontOnu->ReceiveBwAllocation (bwAlloc, nowNano);
			//bwAlloc is received by the tcontONU's queue of XgponXgtcBwAllocation's. A history of 1 second is maintained in the queue

		  //std::cout << "startTime " << startTime << ", nowNano " << nowNano << std::endl;
      if(startTime!=0xFFFF)  //one upstream burst should be scheduled for this ONU now. 
      {
        uint16_t phyFrameSizeBlocks = commonPhy->GetUsPhyFrameSizeInBlocks(); //ja:update:xgspon
        NS_ASSERT_MSG((startTime<phyFrameSizeBlocks), "StartTime is unreasonably large!!!"); 

        uint16_t burstIndex = bwAlloc->GetBurstProfileIndex ();
        const Ptr<XgponBurstProfile>& profile = linkInfo->GetProfileByIndex (burstIndex);
        NS_ASSERT_MSG((profile!=nullptr), "the corresponding burst profile cannot be found!!!");

        //start_time doesn't consider preamble and delimiter
        uint64_t tmpLen = profile->GetPreambleLen () + profile->GetDelimiterLen (); //these units are in bytes
        //std::cout << "tmpLen(Before) : " << tmpLen << std::endl;
        tmpLen = startTime * m_baseGrantSize - tmpLen; //starttime is the time of transmitting xgtcusheader; final unit of tmpLen is bytes, m_baseGrantSize = 4Bytes for XGPON, 16Bytes for XGSPON, ja:update:xgspon
        //std::cout << "tmpLen(after) : " << tmpLen << ", startTime: " << startTime << ", startTime*m_baseGrantSize: " << startTime*m_baseGrantSize << std::endl;

        uint64_t waitTime = 2*linkInfo->GetEqualizeDelay();  //different propagation delay.
        uint64_t txTime = waitTime + (tmpLen * 1000000000L) / commonPhy->GetUsLinkRate();
        //std::cout << "usPhyFrameSize: " << phyFrameSizeBlocks*m_baseGrantSize <<  " Bytes, m_baseGrantSize: " << (int)m_baseGrantSize << "Bytes, bMapSize: " << bwMapSize << ", allocId: " << allocId << ", tmpLen: " << tmpLen << " Bytes, usLinkRate: " << commonPhy->GetUsLinkRate() << " BytesPerSecond, waitTime: " << waitTime << " nanoSeconds" <<  std::endl;
        NS_ASSERT_MSG((txTime<125000), "the scheduled txTime is unreasonably long!!!");
				
        Simulator::Schedule (NanoSeconds(txTime), &XgponOnuNetDevice::ProduceAndTransmitUsBurst, m_device, bwmap, i);
			}
  	}
	}
}








}//namespace ns3
