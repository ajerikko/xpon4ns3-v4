/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c)  2012 The Provost, Fellows and Scholars of the 
 * College of the Holy and Undivided Trinity of Queen Elizabeth near Dublin. 
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
 * Author: Pedro Alvarez <pinheirp@tcd.ie>
 * Author: Jerome Arokkiam <jerome.arokkiam@bt.com>
 */

#include "ns3/log.h"

#include "xgpon-onu-xgem-engine.h"
#include "xgpon-onu-net-device.h"

#include "xgpon-onu-us-scheduler.h"
#include "xgpon-xgem-routines.h"



NS_LOG_COMPONENT_DEFINE ("XgponOnuXgemEngine");

namespace ns3{

NS_OBJECT_ENSURE_REGISTERED (XgponOnuXgemEngine);

TypeId 
XgponOnuXgemEngine::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::XgponOnuXgemEngine")
    .SetParent<XgponOnuEngine> ()
    .AddConstructor<XgponOnuXgemEngine> ()
  ;
  return tid;
}
TypeId 
XgponOnuXgemEngine::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}


XgponOnuXgemEngine::XgponOnuXgemEngine ()
{
}
XgponOnuXgemEngine::~XgponOnuXgemEngine ()
{
}



void
XgponOnuXgemEngine::GenerateFramesToTransmit(std::vector<Ptr<XgponXgemFrame> >& xgemFrames, uint32_t payloadLength, uint16_t allocId)
{
  NS_LOG_FUNCTION(this);

  Ptr<XgponConnectionSender> conn;

  const Ptr<XgponOnuConnManager>& connManager = m_device->GetConnManager ( ); 
  const Ptr<XgponTcontOnu>& tcontOnu = connManager->GetTcontById (allocId);
  const uint16_t tcontOnuType = (uint16_t)tcontOnu->GetTcontType();
	//std::cout << "GENERATING frames for tcontOnuType: " << tcontOnuType << ", with allocId: " << allocId << std::endl;

  NS_ASSERT_MSG((tcontOnuType!=0), "Invalid TCONT Type when sending data from ONU!!!");
  
  const Ptr<XgponOnuUsScheduler>& scheduler = tcontOnu->GetOnuUsScheduler();
  const Ptr<XgponLinkInfo>& linkInfo = (m_device->GetPloamEngine ( ))->GetLinkInfo();


  uint32_t currentPayloadSize, availableSize;
  currentPayloadSize = 0;
  while(currentPayloadSize < payloadLength)
  {
    //ja:update:xgspon unit: all sizes/lengths are bytes 
    availableSize = payloadLength - currentPayloadSize;
    /* ja:xgspon:update
     * a short idle XGEM frame with all-zero bytes are created if the payload section is less than the XGEM header size ( =4 Bytes)
     * an idle XGEM frame is created at the transmitter's own discretion based on the avaialble payload secion. Here, it's between 4 and 15 bytes, inclusive.
     * a payload length of 16 or more Bytes results in a full SDU being created for transmission
    */
    if(availableSize ==4 )  //create a short idle xgem frame (this in only applicable for XGPON; for XGSPON, this condition is met by the end portion of the payload has 4 Bytes left to be transmitted and not when a single block is assigned as minimum with no packets are in the queue.
    {
      xgemFrames.push_back(XgponXgemRoutines::CreateShortIdleXgemFrame ( ));
			//std::cout << " creating SHORT idle Xgem Frame upfront with availableSize:  " << availableSize << std::endl;
      return;
    }
    else if(availableSize<16) //create one idle xgem frame; ja:update:xgspon; for XGSPON (16Byte block size), this is when a status report is sent (4 Bytes) but the remaining block size (12Bytes) needs to be filled; = sign not needed
    {
      xgemFrames.push_back(XgponXgemRoutines::CreateIdleXgemFrame(availableSize));
			//std::cout << " creating IDLE Xgem Frame upfront with availableSize: " << availableSize << std::endl;
      return;
    }
    else //SDUs (if exist) will be encapsulated.
    {
      uint32_t amountToServe;
			//std::cout << " creating PROPER Xgem Frame " << std::endl;    
      conn = scheduler->SelectConnToServe (&amountToServe);
      if(conn==nullptr)  //this T-CONT has no data send. fill with idle XGEM frames
      {
				//std::cout << "\t for a NULL connection with amountToServe: " << amountToServe << ", and VERY BIG availableSize: " << availableSize << std::endl;
        while(availableSize>0)
        {
          if(availableSize > XgponXgemRoutines::XGPON_XGEM_FRAME_MAXLEN) 
          {
            xgemFrames.push_back(XgponXgemRoutines::CreateIdleXgemFrame (XgponXgemRoutines::XGPON_XGEM_FRAME_MAXLEN));
            availableSize = availableSize - XgponXgemRoutines::XGPON_XGEM_FRAME_MAXLEN;
          } 
          else if(availableSize==4)
          {
						xgemFrames.push_back(XgponXgemRoutines::CreateShortIdleXgemFrame ( ));
            return;
          }
          else
          {
            xgemFrames.push_back(XgponXgemRoutines::CreateIdleXgemFrame (availableSize));
            return;
          }
        }
        return;
      }
      else
      {
        bool doSegmentation = false;
        if(amountToServe > (payloadLength - currentPayloadSize)) 
        {
          //the last connection to be served, segmentation will be carried out if it is necessary
          amountToServe = (payloadLength - currentPayloadSize);
          doSegmentation = true;
        }


        Ptr<XgponXgemFrame> frame;
        do
        {
					//std::cout << "\t for a VALID Upstream Connection with amountToServe: " << amountToServe << ", and segmentation: " << doSegmentation << std::endl; 
          frame = XgponXgemRoutines::GenerateXgemFrame (m_device, conn, amountToServe, 
                                       linkInfo->GetCurrentUsKey(), linkInfo->GetCurrentUsKeyIndex(), doSegmentation);
          if(frame!=nullptr)
          {
            xgemFrames.push_back(frame);
            currentPayloadSize += frame->GetSerializedSize();
						amountToServe -= frame->GetSerializedSize();
						//std::cout <<"\t\t and created frame with total currentPayloadSize: " << currentPayloadSize << " , leaving amountToServe: " << amountToServe << std::endl;
            ///////////////////////////////update statistics
            (m_device->GetStatistics()).m_passToXgponBytes += (frame->GetXgemHeader()).GetPli();
            //std::cout << "bytes passed to XGPON upstream: " << (frame->GetXgemHeader()).GetPli() << ", allocId: " << allocId << std::endl;
          }
        } while(amountToServe>16 && frame!=nullptr); //ja:update:xgspon = sign for 16 removed (XGSPON block size is 16 Bytes; so >= 16 would always be executed otherwise
      }
    }
  }  //end of while 

  return;
}



void 
XgponOnuXgemEngine::ProcessXgemFramesFromLowerLayer (std::vector<Ptr<XgponXgemFrame> >& frames)
{
  NS_LOG_FUNCTION(this);


  const Ptr<XgponOnuConnManager>& connManager = m_device->GetConnManager ( ); 

  const uint16_t tcontOnuType = 0;//there's no reason to have tcont types in the downstream since QoS aware DBA is not used in the downstream
  

  //const Ptr<XgponLinkInfo>& linkInfo = (m_device->GetPloamEngine ( ))->GetLinkInfo();
  //used to find the key for decryption


  std::vector<Ptr<XgponXgemFrame> >::iterator it, end;
  it = frames.begin();
  end = frames.end();
  for(;it!=end; it++)
  {
    XgponXgemFrame::XgponXgemFrameType type = (*it)->GetType();
    if(type ==  XgponXgemFrame::XGPON_XGEM_FRAME_WITH_DATA) //For idle XGEM frame, do nothing
    {
      XgponXgemHeader& xgemHeader = (*it)->GetXgemHeader();
      uint16_t portId = xgemHeader.GetXgemPortId ();
      const Ptr<XgponConnectionReceiver>& conn = connManager->FindDsConnByXgemPort(portId);

      if(conn!=nullptr)  //whether this XGEM frame is for this ONU
      {
        const Ptr<Packet>& payload = (*it)->GetData();
        NS_ASSERT_MSG((payload->GetSize() > 0), "Data length should not be zero!!!");

        //Ptr<XgponKey> key = linkInfo->GetDsKeyByIndex (xgemHeader.GetKeyIndex());
        //carry out decryption if needed.
 
        Ptr<Packet> sdu = conn->GetPacket4Reassemble();
        if(sdu!=nullptr) { sdu->AddAtEnd(payload); }
        else { sdu = payload; }

        if(xgemHeader.GetLastFragmentFlag()==0) //save back for further reassemble
        {
          conn->SetPacket4Reassemble (sdu);
        }
        else  //send to upper layer
        {          
          if(portId == m_device->GetOnuId( )) { m_device->GetOmciEngine()->ReceiveOmciPacket(sdu); } //send to OMCI
          else { m_device->SendSduToUpperLayer (sdu, tcontOnuType, 1024, m_device->GetOnuId()); } //send to upper layers          
        } //end for fragmentation state
      } //end for frames whose destination is this ONU   
    } //end for non-idle-frames
  } //end for the loop
  return;
}



}//namespace ns3
