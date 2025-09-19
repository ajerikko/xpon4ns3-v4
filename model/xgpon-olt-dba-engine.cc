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
 * Xiuchao Wu <xw2@cs.ucc.ie>
 * Author: Jerome Arokkiam <jerome.arokkia@bt.com>
 */

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include "xgpon-olt-dba-engine.h"
#include "xgpon-olt-net-device.h"
#include "xgpon-channel.h"



NS_LOG_COMPONENT_DEFINE ("XgponOltDbaEngine");

namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (XgponOltDbaEngine);

TypeId 
XgponOltDbaEngine::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::XgponOltDbaEngine")
    .SetParent<XgponOltEngine> ()
    .AddAttribute("BaseGrantSize",
              "Base Grant size of XGPON/XGSPON (Unit: blocks of word for XGPON or 4-word for XGSPON)",
              UintegerValue(BASE_GRANT_SIZE_XGPON),
              MakeUintegerAccessor(&XgponOltDbaEngine::m_baseGrantSize),
              MakeUintegerChecker<uint8_t>())
    .AddAttribute("FramesPerDBAcycle",//ja:update:xgsponv5
              "No of PON Frames consisting of a single DBA cycle for XGPON/XGSPON (default = 8 frames per DBA Cycle)",
              UintegerValue(4),
              MakeUintegerAccessor(&XgponOltDbaEngine::m_framesPerDBAcycle),
              MakeUintegerChecker<uint8_t>())
  ;
  return tid;
}
TypeId
XgponOltDbaEngine::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}




XgponOltDbaEngine::XgponOltDbaEngine (): m_bursts(), 
  m_aggregateAllocatedSize(0),
  m_servedBwmaps(0), m_nullBwmap(0),
  m_extraInLastBwmap(0),
  m_dsFrameSlotSizeInNano (0), m_logicRtt (0), m_usRate(0)
	//m_framesPerDBAcycle(4),//ja:update:xgsponv5
{
  m_servedBwmaps.clear();
}
XgponOltDbaEngine::~XgponOltDbaEngine ()
{
}







void 
XgponOltDbaEngine::ReceiveStatusReport (const Ptr<XgponXgtcDbru>& report, uint16_t onuId, uint16_t allocId, uint64_t time)
{
  NS_LOG_FUNCTION(this);

  const Ptr<XgponTcontOlt>& tcont = (m_device->GetConnManager( ))->GetTcontById (allocId);
  if(tcont != nullptr)
  {
    //uint64_t nowNano = Simulator::Now().GetNanoSeconds();//ja:update:xgsponv5
		//std::cout << "DBA_timing,receiving SR at,"<< time << ",allocId," << tcont->GetAllocId() << std::endl;
    tcont->ReceiveStatusReport (report, time);
  }
}



const Ptr<XgponXgtcBwmap> 
XgponOltDbaEngine::GenerateBwMap ()  //unit: block of word or 4-word; we assume that multiple-thread dba is not used.
{
  NS_LOG_FUNCTION(this);

  uint64_t nowNano = Simulator::Now().GetNanoSeconds();
  //std::cout << "secondsNano: " << nowNano << std::endl;

	//std::cout << "DBA_timing: Overall cycle (nanoseconds/125000): " << nowNano/125000 << ", DBA Cycle: " << (nowNano%(m_framesPerDBAcycle*125000)/125000) << std::endl;
  const Ptr<XgponPhy>& commonPhy = m_device->GetXgponPhy();
  uint32_t usPhyFrameSize = commonPhy->GetUsPhyFrameSizeInBlocks();
  uint16_t allocatedSize = m_extraInLastBwmap;
  //std::cout << "m_extraInLastBwmap = " << m_extraInLastBwmap << std::endl; //ja:update:xgspon
  NS_ASSERT_MSG((m_extraInLastBwmap < 0.5*(usPhyFrameSize)), "the last bwmap over-allocated too much!!!");
  
	//carry out initialization before the loop
  m_bursts.ClearBurstInfoList( );
  Prepare2ProduceBwmap ( );
	//double perFrameAllocLimitingFactor = 1.5; //ja:update:xgspon

  const Ptr<XgponOltPloamEngine>& ploamEngine = m_device->GetPloamEngine();

  //usPhyFrameSize= 9720 words, equivalent to 2.488Gbps in XGPON, or 9720 4-word blocks for 9.9533Gbps in XGSPON
  //std::cout << "DBA: usPhyFrameSize (in Bytes) = " << usPhyFrameSize*m_baseGrantSize << ", extraFromPrev (Bytes): " << m_extraInLastBwmap*m_baseGrantSize << std::endl; //ja:update:xgspon

  uint16_t guardTime = commonPhy->GetUsMinimumGuardTime (); //TODO: CHECK IF THE US MIN GUARD TIME IS SAME/DIFFERENT IN XG(S)PON
  //std::cout << "guardTime = " << guardTime << std::endl;//ja:update:xgspon verifying that the parameters set at xgpon-helper are effect

  uint32_t numScheduledTconts= 0;
  Ptr<XgponTcontOlt> tcontOlt; 
	//ja:update:xgsponv5 - introducing the idea that a DBA cycle could consist of multiple XG(S)-PON frames. A configurable parameter is introduced in attributes
	if(nowNano%(m_framesPerDBAcycle*125000U) == 0){
		tcontOlt = GetFirstTcontOlt ( );
		//std::cout << "DBA-order: STARTING A DBA CYCLE" << std::endl;
	}else{
		tcontOlt = GetCurrentTcontOlt();
  	//std::cout << "DBA-order: in the MIDDLE of a DBA cycle" << std::endl;
	}

  do 
  {
			//std::cout << "DBA-tcontOlt : " << tcontOlt << std::endl;
    uint32_t size2Assign = 0;
		if(tcontOlt != nullptr){
			size2Assign = CalculateAmountData2Upload (tcontOlt, allocatedSize, nowNano); //this function limits the size2Assign, units in blocks, to be less than maxServiceSize (40KB in XGPON, ~160KB in XGSPON)
			//size2Assign = 1.5 * size2Assign; //ja-tcp-test, what happens to the tcp behaviour if more grant is given than the report
    	//enable the below output to see the details of the serving TCONT, TCONT Type, associated ONU and how much of Bytes requested by the TCONT
	    //std::cout << "DBA-order: for " << tcontOlt->GetTcontType() << "-" << tcontOlt->GetOnuId() << ", calculated size2Assign " << size2Assign*m_baseGrantSize << " Bytes" << std::endl;
		}else{
			//std::cout << "DBA-order: all tconts served in this DBA Cycle " << std::endl;
			break;
		}
    
		//ja:update:xgsponv5 the above function limit the allocation fo an allocID to be less than usPhyFrameSize. This is a significant change to the original xgpon model, when an extra 50% was allowed as allocation as in the commented block below. With multiple frames per DBA cycle, having the commented condition will complicate the frame boundary conditions when within the DBA cycle and at the end of the DBA cycle; hence the blcok below being commented
   	
		//uint32_t largestAssign = perFrameAllocLimitingFactor * usPhyFrameSize - allocatedSize;//ja:update:xgspon5, an extra 20% insted of the earlier 50% to limit overallocation 
    //if(size2Assign > largestAssign) size2Assign = largestAssign;  
			
//    if(GetCurrentTcontOlt() == GetFirstTcontOlt()) {// all T-CONTs had been considered.
//			break;
//		}

    if(size2Assign > 0 && numScheduledTconts < MAX_TCONT_PER_BWMAP)
    {
			//std::cout << "\toltDBA,atMicro," << nowNano/1000 << ",bytesToAssign," << size2Assign*m_baseGrantSize << ",allocId," << tcontOlt->GetAllocId() << std::endl;
			      
			Ptr<XgponOltDbaPerBurstInfo> perBurstInfo = m_bursts.GetBurstInfo4TcontOlt(tcontOlt);      
      if(perBurstInfo!=nullptr)
      {
        SetServedTcont(tcontOlt->GetAllocId());
        const Ptr<XgponLinkInfo>& linkInfo = ploamEngine->GetLinkInfo(tcontOlt->GetOnuId());

        if(perBurstInfo->GetBwAllocNumber() == 0) 
        {
          perBurstInfo->Initialize(tcontOlt->GetOnuId(), linkInfo->GetPloamExistAtOnu4OLT(), linkInfo->GetCurrentProfile(), guardTime,commonPhy->GetUsFecBlockDataSize(), commonPhy->GetUsFecBlockSize(), m_baseGrantSize);
					//std::cout << "Initialise:guardTime," << guardTime << ",FecBlockDataSize," << commonPhy->GetUsFecBlockDataSize() << ",UsFecBlockSize," << commonPhy->GetUsFecBlockSize() << ",baseGrantSize," << (uint16_t)m_baseGrantSize << std::endl;
          //Create the first bwalloc; starttime will be set when producing bwmap from all bursts
          Ptr<XgponXgtcBwAllocation> bwAlloc = Create<XgponXgtcBwAllocation> (tcontOlt->GetAllocId(), true, linkInfo->GetPloamExistAtOnu4OLT(), 0, size2Assign, 0, linkInfo->GetCurrentProfileIndex());
          perBurstInfo->AddOneNewBwAlloc(bwAlloc, tcontOlt, m_baseGrantSize);
					//std::cout << "DBA,burstBytes(firstBwAlloc),"<< perBurstInfo->GetFinalBurstSize() << std::endl;
          allocatedSize += (perBurstInfo->GetFinalBurstSize( ))/m_baseGrantSize;
          numScheduledTconts++;
        }
        else
        {
          if(perBurstInfo->FindBwAlloc(tcontOlt)==nullptr)//bwalloc not already present
          {
            //Create the bwalloc
            Ptr<XgponXgtcBwAllocation> bwAlloc = Create<XgponXgtcBwAllocation> (tcontOlt->GetAllocId(), true, linkInfo->GetPloamExistAtOnu4OLT(), 0xFFFF, size2Assign, 0, 0);
            uint32_t orgBurstSize = perBurstInfo->GetFinalBurstSize( );
						//std::cout << "DBA,burstBlocks(newBwAlloc),before,"<< orgBurstSize << std::endl;
            perBurstInfo->AddOneNewBwAlloc(bwAlloc, tcontOlt, m_baseGrantSize);
            allocatedSize += (perBurstInfo->GetFinalBurstSize( ) - orgBurstSize)/m_baseGrantSize;
						//std::cout << "DBA,addedBurstBlocks,after"<< allocatedSize - m_extraInLastBwmap << std::endl;
            numScheduledTconts++;
          }
          else
          {
            //BwAllocation already exists, add extra allocation to the existing bwAlloc
            Ptr<XgponXgtcBwAllocation> bwAlloc = perBurstInfo->FindBwAlloc(tcontOlt);
            uint32_t orgBurstSize = perBurstInfo->GetFinalBurstSize( );
						//std::cout << "DBA,burstBlocks(newBwAlloc),before,"<< orgBurstSize << std::endl;
            perBurstInfo->AddToExistingBwAlloc( bwAlloc, size2Assign*m_baseGrantSize);
            allocatedSize += (perBurstInfo->GetFinalBurstSize( ) - orgBurstSize)/m_baseGrantSize;
						//std::cout << "DBA,addedBurstBlocks,after"<< allocatedSize - m_extraInLastBwmap << std::endl;
            //do not increment numScheduledTconts
          }
        }
      }
    }    


		//std::cout << "allocatedSize," << allocatedSize << ",usPhyFrameSize," << usPhyFrameSize << std::endl;
    tcontOlt = GetNextTcontOlt ( );
    if(CheckAllTcontsServed()) {// all T-CONTs had been considered.
			//std::cout << "\tDBA-order: ALL TCONTS SERVED IN THE FRAME " << std::endl;
      break;
		}

      
  } while((allocatedSize < (usPhyFrameSize-10)) && numScheduledTconts<MAX_TCONT_PER_BWMAP);

	//std::cout << "DBA-order,at_time," << nowNano << ",nanoSeconds,DBA_Cycle," << (nowNano%(m_framesPerDBAcycle*125000)/125000) << ",blockSize," << (uint16_t)m_baseGrantSize << ",Bytes,totalAllocBlocks," << allocatedSize << ",usPHYblocks," << usPhyFrameSize << ",numSchTcontsThisFrame," << numScheduledTconts << ",extraBlocks," << m_extraInLastBwmap << std::endl;
  //TODO: assert m_minimumSI >= 1
  m_aggregateAllocatedSize += allocatedSize;
  //std::cout << "Total AllocatedSize: " << allocatedSize*m_baseGrantSize << " Bytes" << std::endl;
	//std::cout << "nextT4threshold ALLOCATION CYCLE END" << std::endl;
      

  //To make sure that starttime in the bwalloc is less than usPhyFrameSize. 
  //Otherwise, OLT may not find the corresponding BWMAP for this burst.
  //10*4 = 40bytes > gap+preamble+delimiter

	

  Ptr<XgponXgtcBwmap> map = m_bursts.ProduceBwmapFromBursts(nowNano, m_extraInLastBwmap, usPhyFrameSize, m_baseGrantSize);
  //if(map->GetNumberOfBwAllocation() > 0) map->Print(std::cout);

  if(allocatedSize > usPhyFrameSize)  //update the over-allocation size
  {  
    m_extraInLastBwmap = allocatedSize - usPhyFrameSize; //all units are in blocks (4 Bytes for XGPON and 16 Bytes for XGSPON)
  } else m_extraInLastBwmap = 0;

  map->SetCreationTime(nowNano);
  m_servedBwmaps.push_back(map);  //used for receiving the corresponding bursts

  FinalizeBwmapProduction();
	//std::cout << "\t\tDBA:bw_map_finalised,at_time," << nowNano << ",totalAllocBytes," << allocatedSize*m_baseGrantSize << ",m_extraInLastBwmapBytes," << m_extraInLastBwmap*m_baseGrantSize << std::endl;
  return map;
	
}









const Ptr<XgponXgtcBwmap>& 
XgponOltDbaEngine::GetBwMap4CurrentBurst (uint64_t time)
{
  NS_LOG_FUNCTION(this);

  uint64_t slotSize = GetFrameSlotSize ( );  //slotSize in nanosecond
  uint64_t rtt = GetRtt();

  while(m_servedBwmaps.size() > 0)
  {
    const Ptr<XgponXgtcBwmap>& first = m_servedBwmaps.front();
    uint64_t startTime = first->GetCreationTime() + rtt;
    uint64_t endTime = startTime + slotSize;

    NS_ASSERT_MSG((time>startTime), "The corresponding bwmap was deleted too early!!!"); 

    if(time < endTime) return m_servedBwmaps.front();
    else
    { //first bwmap should be deleted since all of the corresponding bursts have been received (based on time).
      m_servedBwmaps.pop_front();
    }
  }

  NS_ASSERT_MSG(false, "this line should never been run!!!"); 
  return m_nullBwmap;
}


const Ptr<XgponBurstProfile>& 
XgponOltDbaEngine::GetProfile4BurstFromChannel(uint64_t time)
{
  NS_LOG_FUNCTION(this);

  Ptr<XgponXgtcBwmap> map = GetBwMap4CurrentBurst (time);
  NS_ASSERT_MSG((map!=nullptr), "There is no corresponding bwmap for this burst!!!");

  uint32_t first = GetIndexOfBurstFirstBwAllocation (map, time);
  NS_ASSERT_MSG((first < map->GetNumberOfBwAllocation()), "strange index of the corresponding bwallocation!!!");

  const Ptr<XgponXgtcBwAllocation>& bwAlloc = map->GetBwAllocationByIndex (first);

  uint16_t onuId = ((m_device->GetConnManager( ))->GetTcontById (bwAlloc->GetAllocId() ))->GetOnuId();

  return ((m_device->GetPloamEngine())->GetLinkInfo (onuId))->GetProfileByIndex(bwAlloc->GetBurstProfileIndex());  
}



uint32_t 
XgponOltDbaEngine::GetIndexOfBurstFirstBwAllocation (const Ptr<XgponXgtcBwmap>& bwmap, uint64_t time)
{
  NS_LOG_FUNCTION(this);

  //calculate STARTTIME based on receiving time of the burst
  // (offsetTime * usRate)/(4*1000000000)
  uint64_t offsetSize64 = (time - (bwmap->GetCreationTime ( ) + GetRtt())) * GetUsLinkRate();   
  uint64_t tmpInt64 = 1000000000L;
  offsetSize64 = offsetSize64 / (m_baseGrantSize*tmpInt64); //ja:update::xgspon replaced the word size with the actual block size (4 or 16) as per the PON technology (XGPON or XGSPON respectively)
  uint32_t offsetSize = (uint32_t) offsetSize64;  //unit: blocks of word or 4-words

  int num = bwmap->GetNumberOfBwAllocation();
  for(int i=0; i<num; i++)
  {
    uint32_t startTime = (bwmap->GetBwAllocationByIndex(i))->GetStartTime();

    //Note that the starttime in BwAlloc starts from the XGTC header (after the preamble and delimiter). Thus, the burst will arrive before the starttime.
    if((startTime!= 0xFFFF) && offsetSize < startTime ) { return i; }
  }
  
  NS_ASSERT_MSG(false, "this line should never been run!!!"); 
  return (num+1); //return one impossible value.
}





















void 
XgponOltDbaEngine::PrintAllActiveBwmaps (void) 
{
  std::list< Ptr<XgponXgtcBwmap> >::iterator it = m_servedBwmaps.begin(); 
  std::list< Ptr<XgponXgtcBwmap> >::iterator end = m_servedBwmaps.end();

  while(it!=end)
  {
    std::cout << std::endl << std::endl;
    (*it)->Print(std::cout);
    std::cout << std::endl << std::endl;
    it++;
  }
}




uint32_t
XgponOltDbaEngine::GetRtt ()
{
  if(m_logicRtt == 0)
  {
    const Ptr<XgponChannel>& ch =  DynamicCast<XgponChannel, Channel>(m_device->GetChannel());
    m_logicRtt = 2 * (ch->GetLogicOneWayDelay ());
  }
  return m_logicRtt;
}

uint32_t
XgponOltDbaEngine::GetFrameSlotSize ()
{
  if(m_dsFrameSlotSizeInNano == 0)
  {
    m_dsFrameSlotSizeInNano = (m_device->GetXgponPhy( ))->GetDsFrameSlotSize ( );  //slotSize in nanosecond
  }
  return m_dsFrameSlotSizeInNano;
}

uint32_t
XgponOltDbaEngine::GetUsLinkRate ()
{
  if(m_usRate==0)
  {
    m_usRate = (m_device->GetXgponPhy ( ))->GetUsLinkRate();
  }
  return m_usRate;
}


bool
XgponOltDbaEngine::CheckServedTcont(uint64_t allocId)
{
  return m_bursts.CheckServedTcont(allocId);
}

void
XgponOltDbaEngine::SetServedTcont(uint64_t allocId)
{
  m_bursts.SetServedTcont(allocId);
}



}//namespace ns3
