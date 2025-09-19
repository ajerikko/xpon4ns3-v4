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
 * Jerome Arokkiam <jerome.arokkiam@bt.com>
 */

#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"

#include "xgpon-olt-dba-engine-round-robin.h"




NS_LOG_COMPONENT_DEFINE ("XgponOltDbaEngineRoundRobin");

namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (XgponOltDbaEngineRoundRobin);

TypeId 
XgponOltDbaEngineRoundRobin::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::XgponOltDbaEngineRoundRobin")
    .SetParent<XgponOltDbaEngine> ()
    .AddConstructor<XgponOltDbaEngineRoundRobin> ()
    .AddAttribute ("MaxPollingInterval", 
                   "The maximal interval to poll one T-CONT (Unit: nano-second).",
                   UintegerValue (XgponOltDbaEngineRoundRobin::XGPON1_MAX_POLLING_INTERVAL),
                   MakeUintegerAccessor (&XgponOltDbaEngineRoundRobin::m_maxPollingInterval),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxServiceSize", 
                   "The maximal number of words that could be allocated to one T-CONT in a bandwidth allocation (Unit: word).",
                   UintegerValue (XGPON1_US_PER_SERVICE_MAX_SIZE),
                   MakeUintegerAccessor (&XgponOltDbaEngineRoundRobin::m_maxServiceSize),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}
TypeId
XgponOltDbaEngineRoundRobin::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}




XgponOltDbaEngineRoundRobin::XgponOltDbaEngineRoundRobin () : XgponOltDbaEngine(),
  m_lastSchTcontIndexForFrame(0),
	m_lastSchTcontIndexForCycle(0),
	m_dbaCycleStart(false),
	//m_nullTcont(0),
  m_getNextTcontAtBeginning(false)
{
  m_usAllTconts.clear();
}
XgponOltDbaEngineRoundRobin::~XgponOltDbaEngineRoundRobin ()
{
}




void 
XgponOltDbaEngineRoundRobin::AddTcontToDbaEngine (Ptr<XgponTcontOlt>& tcont)
{
  NS_LOG_FUNCTION(this);

  m_usAllTconts.push_back(tcont);
  return;
} 



const Ptr<XgponTcontOlt>& 
XgponOltDbaEngineRoundRobin::GetNextTcontOlt ( )
{
  NS_LOG_FUNCTION(this);
  
	m_lastSchTcontIndexForFrame++;
  if(m_lastSchTcontIndexForFrame >= m_usAllTconts.size()) m_lastSchTcontIndexForFrame = 0;
 	//std::cout << "DBA-order: getting next tcont index " << m_lastSchTcontIndexForFrame << std::endl; 
	return m_usAllTconts[m_lastSchTcontIndexForFrame];
}

const Ptr<XgponTcontOlt>& 
XgponOltDbaEngineRoundRobin::GetCurrentTcontOlt ( ) 
{
  NS_LOG_FUNCTION(this);
 	//std::cout << "DBA-order: getting current tcont index for frame: " << m_lastSchTcontIndexForFrame << std::endl;
 
	if(m_dbaCycleStart){
		return m_usAllTconts[m_lastSchTcontIndexForFrame];
	} else {
		return m_nullTcont;
	}
}

const Ptr<XgponTcontOlt>&
XgponOltDbaEngineRoundRobin::GetFirstTcontOlt ( )
{
	m_dbaCycleStart = true;
  m_firstTcontOlt = m_usAllTconts[m_lastSchTcontIndexForCycle];
	m_lastSchTcontIndexForFrame = m_lastSchTcontIndexForCycle;
  NS_ASSERT_MSG((m_firstTcontOlt!=nullptr), "There is no T-CONT in the network!!!");
 	//std::cout << "DBA-order: first tcont index for cycle/frame: " << m_lastSchTcontIndexForCycle << std::endl;
  return m_firstTcontOlt;
}

bool
XgponOltDbaEngineRoundRobin::CheckAllTcontsServed ( )
{
  NS_LOG_FUNCTION(this);
  
  m_lastSchTcontIndexForCycle = m_lastSchTcontIndexForFrame;
  if(m_lastSchTcontIndexForCycle >= m_usAllTconts.size()) m_lastSchTcontIndexForCycle = 0;

	//std::cout << "DBA: CheckingAllTcontsServed: m_lastSchTcontIndexForCycle: " << m_lastSchTcontIndexForCycle << std::endl;
  if(m_firstTcontOlt == m_usAllTconts[m_lastSchTcontIndexForCycle])
  {
    /*******************************************************************
     * In this case, all T-CONTs are processed in the same BWmap.
     * Although the cursor has been moved at the last step, but the tcont isn't served.
     * For starting from the same t-cont in the next round,
     * one flag should be used to notify OLT that GetNextTcontOlt should not
     * be called at the beginning of the next round.
     *******************************************************************/
		m_dbaCycleStart = false;		
    return true; //to break the DBA loop
  }
  else
    return false;
}


uint32_t 
XgponOltDbaEngineRoundRobin::CalculateAmountData2Upload (const Ptr<XgponTcontOlt>& tcontOlt,	uint32_t allocatedSize, uint64_t nowNano)
{
  uint32_t size2Assign = tcontOlt->CalculateRemainingDataToServe(GetRtt(), GetFrameSlotSize()); //unit: blocks (4 Bytes in XGPON, 16 Bytes in XGSPON)
  

  //ja:update:xgsponv5; in RR DBA, n_ONUs = m_usAllTconts.size(), m_maxServiceSize is in Blocks here (9718)
	uint16_t overheadPerONU = 188; //ja:update:xgsponv5
	uint32_t largest2Assign = ((m_framesPerDBAcycle * m_maxServiceSize)/ m_usAllTconts.size()) - overheadPerONU; //ja:update:xgsponv5, largest value allowed for an allocId (an ONU in the RR DBA) is limited by the burst info overhead 
	
	//std::cout << "DBA,size2AssignReq," << size2Assign*m_baseGrantSize << "(Bytes),(theRestInBlocks),largest2Assign,"<< largest2Assign << ",m_framesPerDBAcyele," << (int)m_framesPerDBAcycle << ",nTconts: " << m_usAllTconts.size() << ",maxServiceSize," << m_maxServiceSize << std::endl;

	if(size2Assign > 0)
  {
    //always allow the t-cont to piggyback queue status report with its upstream data
    size2Assign += 1;      //one word for queue status report
    if(size2Assign > largest2Assign)	size2Assign = largest2Assign; //ja:update:xgsponv5
		else if(size2Assign < 4) size2Assign  =4;    //smallest allocation for receiving data from ONU: TODO: ja:update:xgspon find out if the smallest allocation is 4 blocks or 16 bytes (if it's the latter, then 16 bytes is equivalent to 4 blocks in XGPON but 1 block in XGSPON.
  }
  else
  {
    //whether to poll this t-cont for its queue status
    //uint64_t lastPollingTime = tcontOlt->GetLatestPollingTime();
    //if((nowNano - lastPollingTime) > m_maxPollingInterval) 
    //give a reporting opportunity to ONU, when it doesnt have data to upload. 
    //This will be useful to calculate the overhead introduced by DBA engine
    size2Assign = 1;
  }

	
  //std::cout << "DBA: size2Assign: (in Bytes, after) " << size2Assign*m_baseGrantSize  << std::endl;
  return size2Assign;
}


void 
XgponOltDbaEngineRoundRobin::Prepare2ProduceBwmap ( )
{
}

void
XgponOltDbaEngineRoundRobin::FinalizeBwmapProduction ()
{
}


}//namespace ns3
