/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
 * Author: Jerome Arokkiam <jerome.arokkiam@bt.com>
 */

/**********************************************************************
* ARCHITECTURE:
*  	     core---------metro---------------lastmile-----edge
*  
*  [SERVER]----[ROUTER]---[OLT]-------[ONU]------[EDGE]____[USER]
*            |                    |                     |
*  [SERVER]--|                    |                     |__[USER]
*            |                    |
*  [SERVER]--|           	        |___[ONU]------[EDGE]____[USER]  
*            |                                          |
*  [SERVER]__|                                          |__[USER]
*
* More details of this XG(S)-PON module can be found at the reference:
* Arokkiam, J. A., Alvarez, P., Wu, X., Brown, K. N., Sreenan, C. J., Ruffini, M., … Payne, D. (2017). Design, implementation, and evaluation of an XG-PON module for the ns-3 network simulator. SIMULATION, 93(5), 409–426. https://doi.org/10.1177/0037549716682093
*
* The latest XG-PON code can be downloaded at https://github.com/ajerikko/xgpon4ns3-v3/releases/tag/v3.1 
**************************************************************/

#include <ctime>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <math.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/object-factory.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/packet-sink.h"
#include "ns3/tcp-socket.h"

#include "ns3/xgpon-helper.h"
#include "ns3/xgpon-config-db.h"

#include "ns3/xgpon-channel.h"
#include "ns3/xgpon-onu-net-device.h"
#include "ns3/xgpon-olt-net-device.h"

#include "ns3/xgpon-module.h"

//Stats module for tracing
#include "ns3/stats-module.h"



//#define APP_START 0.5
#define APP_STOP 5   
#define SIM_STOP APP_STOP +0.5


using namespace ns3;

//global parameters
static const uint16_t nOnus = 2; //number of ONUs to be used in the XGPON
static const uint16_t nFlwOnu = 5; //number of application traffic flows per ONU
static const uint32_t timeIntervalToPrint = 100000000; //1,000,000,000 nanoseconds per second; so this prints the traces every 100ms
static uint64_t time2printOnu[nOnus] = {timeIntervalToPrint};
static uint64_t time2printOlt = timeIntervalToPrint;
static uint16_t incrementingOnuId = 0;
static uint64_t totalOnuReceivedBytes = 0;

NS_LOG_COMPONENT_DEFINE ("xgpon-dba-tcp-basic");

//trace at the OLT for upstream (counters are updated at the receiver)
void
DeviceStatisticsTrace (const XgponNetDeviceStatistics& stat)
{
  if(stat.m_currentTime > time2printOlt)
  {
    uint64_t totalRxFromXgponBytes = 0;
    //std::cout << "stat.m_currentTime: " << stat.m_currentTime << std::endl;
    for(uint16_t i = 0; i < nOnus; i++){ 
      totalRxFromXgponBytes += stat.m_usOltBytes[i];
      //RoundRobin DBA only increases the counters for T4. QoS aware DBAs (all others except RoundRobin) generate traffic in all TCONTS; but since the traffic is generated for T2 - T4 in this example by default, only three types of counters can be seen incremented in the upstream when using the QoS-aware DBAs.
      std::cout << (stat.m_currentTime / 1000000L) << ",ms," 
          << "From ONU," << i << ",total-Upstream," << stat.m_usOltBytes[i] << ","
          << "alloc," << i+1024 << ",T1," << stat.m_usT1oltBytes[i] << ","
          << "alloc," << i+2048 << ",T2," << stat.m_usT2oltBytes[i] << ","
          << "alloc," << i+3072 << ",T3," << stat.m_usT3oltBytes[i] << "," 
          << "alloc," << i+4096 << ",T4," << stat.m_usT4oltBytes[i] << "," << std::endl;
    }   
    //std::cout << "\tallONU-total-Upstream," << totalRxFromXgponBytes << ",(Bytes)" << std::endl;
    time2printOlt = stat.m_currentTime + 100000000; //increment by 100ms
  }
}

//trace at the ONUs for the downstream (counters are updated at the receiver)
void
DeviceStatisticsTraceOnu (const XgponNetDeviceStatistics& stat)
{
  if(stat.m_currentTime > time2printOnu[incrementingOnuId])
  {
    totalOnuReceivedBytes += stat.m_dsOnuBytes;
    std::cout << (stat.m_currentTime / 1000000L) << ",ms,ONU," << incrementingOnuId << ",total-Downstream-thisOnu," << stat.m_dsOnuBytes << ",Bytes" << std::endl;
    time2printOnu[incrementingOnuId] += 100000000;
    incrementingOnuId++;
    if(incrementingOnuId == nOnus){
      std::cout << (stat.m_currentTime / 1000000L) << ",ms,total-Downstream-allOnus," << totalOnuReceivedBytes << std::endl;
      totalOnuReceivedBytes = 0;
      incrementingOnuId = 0;
    }
  }
}

static uint32_t
GetNodeIdFromContext(std::string context)
{
    const std::size_t n1 = context.find_first_of('/', 1);
    const std::size_t n2 = context.find_first_of('/', n1 + 1);
    return std::stoul(context.substr(n1 + 1, n2 - n1 - 1));
}

static void
CwndTracer(std::string context, uint32_t oldval, uint32_t newval)
{
	
	std::cout << "cwndChange,at_time_tcp," << Simulator::Now().GetNanoSeconds() << ",node," << GetNodeIdFromContext(context) << ",newCwnd," << newval << std::endl;
}

void
ConnectSocketTraces()
{
	Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",MakeCallback(&CwndTracer));
}



int 
main (int argc, char *argv[])
{

	//LogComponentEnable ("TcpL4Protocol", LOG_LEVEL_LOGIC);
	//LogComponentEnable ("XgponOltNetDevice", LOG_LEVEL_FUNCTION);
	//LogComponentEnable ("XgponOnuNetDevice", LOG_LEVEL_FUNCTION);
	//LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_INFO);
	//LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
	//LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
	//LogComponentEnable ("TcpCubic", LOG_LEVEL_LOGIC);
	//LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_FUNCTION);
	//LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_FUNCTION);
	//LogComponentEnable ("TcpSocketBase", LOG_LEVEL_LOGIC);

 
	/////////////////////////////////////SETTING DYNAMIC PARAMETERS FOR THE OVERALL SIMULATION
  //these are example values that may need to be changed often, and may impact multiple configuration locations in this example file. 
  
  std::string pon_mode = "XGSPON"; //pon_mode = XGPON or XGSPON; default is XGPON
  std::string traffic_direction = "downstream"; //traffic direction: downstream/upstream; default is upstream
  std::string upstream_dba = "RoundRobin"; //DBA to be used for upstream bandwidth allocation
  std::string per_app_rate = "50Mbps"; //Datarate of an application traffic source
  uint16_t udp_packet_size = 1436; //packet size to be used with UDP applications; in TCP, segment size takes effect 
	uint32_t tcp_segment_size = 1400; //tcp segment size
	//uint16_t dtqSize=800; //queue size for the net devices used in this example. Per AllocID Queues needs to be set at xgpon-queue.cc
  /*  
   *  DBA mechanisms that can be used are: RoundRobin, Giant, Ebu, Xgiant, XgiantDeficit, XgiantProp
      
      NOTE: RoundRobin is a non-QoS aware DBA mechanism (therefore, not compliant with the G987.3 standard). This DBA is there for verification purposes.
      Parameters in section 'CONFIGURATIONS FOR QOS-AWARE DBAS ONLY' do not impact the behaviour of RoundRobin DBA.
      The rest of the DBAs are QoS-aware DBA mechanisms (section CONFIGURATIONS FOR QOS-AWARE DBAS ONLY is responsible only for the outcome of these QoS-aware DBAs)
      One can look at the relevant implemenation files of RoundRobin and Xgiant to see the difference in implementations between a non-QoS aware and QoS-aware DBAs.
  */

  CommandLine cmd;
  //COMMAND LINE OPTIONS FOR THE USER TO OVERRIDE THE DEFAULT OPTIONS ABOVE
  cmd.AddValue ("pon-mode", "Select the PON technology to be used in the simualtion [XGPON, XGSPON] (default XGSPON)", pon_mode);
  cmd.AddValue ("traffic-direction", "The direction of the traffic flow in XG(S)PON [downstream, upstream] (default upstream)", traffic_direction);
  cmd.AddValue ("upstreamDBA", "DBA to be used for XG(S)PON upstream; a simple RoundRobin is used for downstream [RoundRobin, Giant, Ebu, Xgiant, XgiantDeficit, XgiantProp] (default RoundRobin)", upstream_dba);
  cmd.AddValue("app-rate", "Datarate of an application traffice source (values: 10Mbps, 1Gbps, 254kbps, etc)", per_app_rate);
  cmd.AddValue("udp-packet-size", "UDP Packet size", udp_packet_size);
  cmd.AddValue("tcp-segment-size", "TCP Segment size", tcp_segment_size);
	cmd.Parse (argc, argv);
 
  std::string xgponDba = "ns3::XgponOltDbaEngine";
  xgponDba.append(upstream_dba);

	std::cout << "Parameters used:" << std::endl;
	std::cout << "\tpon-mode: " << pon_mode << "\n\ttraffic-direction: " << traffic_direction << "\n\tupstreamDBA: " << upstream_dba << "\n\tUDP per-app-rate:" << per_app_rate << 
				"\n\tno.of.flows: " << nFlwOnu << 
				"\n\tTCP Segment Size: " << tcp_segment_size << std::endl;

  ////////////////////////////////////////////////////////CONFIGURATIONS FOR QOS-AWARE DBAS ONLY
  
  /* 2.15 is the ideal max capacity, as the DBAoverhead(~4Mbps) + datarate(692Mbps) for T4 equals that of T2 and T3 datarates(696.7Mbps) individually. 
   * The additional ~0.7Mbps can be attributed to XGPON overhead in the seperate grant given to T4 every cycle, in the firstT3round
   * One is allowed to set higher values here, but the outcome will be an overprovisioning DBA mechanism. 
   * Note that this configuration is only for DBA algorithm operation, and does not impact the actural capacity of XG-PON which is controlled through how much of bytes packed in each XGPON frame.
   * Indirectly for Upstream however, DBA controlls how much payload goes into each Upstream frame. Hence one must take care in altering values set here when using QoS-aware DBAs. 
   * RoundRobin DBA is not affected by these values.
   */
  double max_bandwidth = ((traffic_direction == "upstream") & (pon_mode == "XGPON")) ? 2.24 : 9.94; //Capacity of XGPON in upstream is 2.24Gbps; for all other cases (XGSPON up/downstream and XGPON downstrea), it is 9.94 Gbps 
  uint32_t siValue=1;//service interval for GIR. 
  
  std::vector<uint64_t> fixedBw(nOnus);
  std::vector<uint64_t> assuredBw(nOnus);
  std::vector<uint64_t> nonAssuredBw(nOnus);
  std::vector<uint64_t> bestEffortBw(nOnus);
  std::vector<uint64_t> maxBw(nOnus);
  std::vector<uint16_t> siMax(nOnus);
  std::vector<uint16_t> siMin(nOnus);

  //these are aggregate network load values for each TCONT type for the entire XG-PON.
  //each tcont in each ONU will get its propotional value, as can be seen in the corresponding DBA implementation files
  uint32_t fixedBwValue=0.0*max_bandwidth*1e9; //unit: bps, e.g. 0.7*max_bandwidth*1e9 means 0.7*2.15Gbps
  uint32_t assuredBwValue=0.7*max_bandwidth*1e9;	
  uint32_t nonAssuredBwValue=0.8*max_bandwidth*1e9;	
  uint32_t bestEffortBwValue=0.67*max_bandwidth*1e9;

  for(uint32_t i=0; i<nOnus; i++)
  {
    fixedBw[i]=fixedBwValue/nOnus;
    assuredBw[i]=assuredBwValue/nOnus;
    nonAssuredBw[i]=nonAssuredBwValue/nOnus;
    bestEffortBw[i]=bestEffortBwValue/nOnus;
    siMax[i]=siValue;
    siMin[i]=2*siValue; //conversion from GIR to PIR service interval
  }
  
  //////////////////////////////////////////////////////////////////////////CONFIGURATIONS FOR XGPON HELPER
  XgponHelper xgponHelper;
  XgponConfigDb& xgponConfigDb = xgponHelper.GetConfigDb ( );

  xgponConfigDb.SetPonMode(pon_mode); //ja:update:xgspon setting the mode for XGSPON (or XGPON by default)
  xgponConfigDb.SetOltNetmaskLen (8);
  xgponConfigDb.SetOnuNetmaskLen (24);
  //XGPON IPBASE
  xgponConfigDb.SetIpAddressFirstByteForXgpon (10);
  //LASTMILE IPBASE
  xgponConfigDb.SetIpAddressFirstByteForOnus (173);
  xgponConfigDb.SetAllocateIds4Speed (true);
  xgponConfigDb.SetOltDbaEngineTypeIdStr (xgponDba); 
  
  //Set TypeId String and other configuration related information through XgponConfigDb before the following call.
  xgponHelper.InitializeObjectFactories ( );


  ////////////////////////////////////////////////////////CONFIGURATIONS FOR POSSIBLE DATARATE VALUES FOR P2P LINKS AND APPLICATION (NOT FOR XGPON)
  
	//XG-PON is the only bottleneck; hence the rest of the network capacity and the application traffic are overpro    visioned 
  double corePerNodeP2PDataRate = max_bandwidth*3;
  double metroNodesP2PDataRate = max_bandwidth*nOnus*5; //because metro aggregates all servers/users in a single link
  double lastMilePerNodeP2PDataRate = max_bandwidth*3;
  //double edgePerNodeP2PDataRate = max_bandwidth*3; 

  //p2p datarate for core nodes
  std::stringstream corePerNodeP2PDataRateString;
  corePerNodeP2PDataRateString << corePerNodeP2PDataRate << "Gbps";
  //p2p datarate for the metro nodes
  std::stringstream metroNodesP2PDataRateString;
  metroNodesP2PDataRateString << metroNodesP2PDataRate << "Gbps"; 
  //p2p datarate for and lastMile, edge nodes
  std::stringstream lastMilePerNodeP2PDataRateString;
  lastMilePerNodeP2PDataRateString << lastMilePerNodeP2PDataRate << "Gbps";
  std::stringstream edgePerNodeP2PDataRateString;
  edgePerNodeP2PDataRateString << lastMilePerNodeP2PDataRate << "Gbps";

   /*  
    //if an autonomous method is needed to set the application datarate, here's an example
    //siValue and load are necessesary multipliers
    double load=0.5; //loading of the XGPON by the application, in terms of the ratio againts the XGPON capacity. this can be anywhere between 0 and 2.0
    double perAppDataRate = siValue*max_bandwidth*load/(nOnus*3); //due to having three tconts per ONU
    std::stringstream perAppDataRateString;
    perAppDataRateString << perAppDataRate << "Gbps";
   */
  std::cout << "datarate(Gbps),metro," << metroNodesP2PDataRateString.str() << ",core," << corePerNodeP2PDataRateString.str() << ",lastmile," << lastMilePerNodeP2PDataRateString.str() << ",edge," << edgePerNodeP2PDataRateString.str()<< ",perApplication," << per_app_rate << std::endl;

  //////////////////////////////////////////CREATE the ONU AND OLT NODES
  Packet::EnablePrinting ();
  
  NodeContainer oltNode, onuNodes;
  //uint16_t nOnus = noOfOnus;  //Number Of Onus in the PON
  oltNode.Create (1);
  onuNodes.Create (nOnus);

  NodeContainer xgponNodes;
  xgponNodes.Add(oltNode.Get(0));
  for(int i=0; i<nOnus; i++) { xgponNodes.Add (onuNodes.Get(i)); }
  
  //0: olt; i (>0): onu
  NetDeviceContainer xgponDevices = xgponHelper.Install (xgponNodes);

  //////////////////////////////////////////SETTING UP OTHER P2P LINKS IN THE NETWORK
  NodeContainer serverNodes, userNodes, routerNodes, edgeNodes; //edgeNodes are introduced to decouple user interfaces with ONU interfaces to the Edge device (e.g. Wi-Fi AP/edge Router, etc.)
	userNodes.Create (nFlwOnu*nOnus);
  serverNodes.Create (nFlwOnu*nOnus);
  edgeNodes.Create (nOnus);
  routerNodes.Create(1);
  
  NetDeviceContainer p2pLastMileDevices[nOnus], p2pMetroDevices, p2pCoreDevices[nFlwOnu*nOnus], p2pEdgeDevices[nFlwOnu*nOnus];
  NodeContainer p2pLastMileNodes[nOnus], p2pMetroNodes, p2pCoreNodes[nFlwOnu*nOnus], p2pEdgeNodes[nFlwOnu*nOnus];
  Ipv4InterfaceContainer p2pLastMileInterfaces[nOnus], p2pMetroInterfaces[nOnus], p2pCoreInterfaces[nFlwOnu*nOnus], p2pEdgeInterfaces[nFlwOnu*nOnus];
  std::vector<uint16_t > allocIdList;


  PointToPointHelper pointToPoint;

  //set the maximum no of packets in p2p tx queue
  //Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue(800));

  std::string p2pDelay = "1ms"; //p2p delay beween diffent network segments used in this example. This is not the XG-PON OLT-ONU delay. For setting the XG-PON propagation dealy, one should look at xgpon-channel.h 
  pointToPoint.SetChannelAttribute ("Delay", StringValue (p2pDelay));
  Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue (1500)); //default is 1500
  
  
  //INSTALL INTERNET PROTOCOL STACK
  InternetStackHelper stack;
  stack.Install (xgponNodes);
  stack.Install (routerNodes);
  stack.Install (serverNodes);
  stack.Install (edgeNodes);
  stack.Install (userNodes);
  
	//core and edge containers
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue(corePerNodeP2PDataRateString.str()));
  int k;
  for(int i=0; i<nOnus; i++){
		for(int j=0; j<nFlwOnu; j++){
			k = i*nFlwOnu + j;
			p2pCoreNodes[k].Add(serverNodes.Get(k));
			//std::cout << "routingDebug: serverNode[" <<j<<"]"<< " in ONU " << i << " id: " << serverNodes.Get(k)->GetId() << std::endl;
			p2pCoreNodes[k].Add(routerNodes.Get(0)); //no. of ONUs remain the same as the no. of flows per ONU scale
			p2pCoreDevices[k] = pointToPoint.Install (p2pCoreNodes[k]);
		}
	}
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue(edgePerNodeP2PDataRateString.str()));
  for(int i=0; i<nOnus; i++){
		for(int j=0; j<nFlwOnu; j++){
		  k = i*nFlwOnu + j;
      p2pEdgeNodes[k].Add(userNodes.Get(k));
			//std::cout << "routingDebug: userNode[" <<j<<"]"<< " in ONU " << i << " id: " << userNodes.Get(k)->GetId() << std::endl;
    	p2pEdgeNodes[k].Add(edgeNodes.Get(i)); //no. of ONUs remain the same as the no. of flows per ONU scale
  	  p2pEdgeDevices[k] = pointToPoint.Install (p2pEdgeNodes[k]);
		}
  }
 
  //lastmile containers
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue(lastMilePerNodeP2PDataRateString.str()));
  for(int i=0; i<nOnus; i++){
    p2pLastMileNodes[i].Add(edgeNodes.Get(i));
  	p2pLastMileNodes[i].Add(onuNodes.Get(i)); //no. of ONUs remain the same as the no. of flows per ONU scale
	  p2pLastMileDevices[i] = pointToPoint.Install (p2pLastMileNodes[i]);
  }
  

  //metro containers
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue(metroNodesP2PDataRateString.str()));
  //for(int i=0; i<nOnus; i++){
		p2pMetroNodes.Add(routerNodes.Get(0));
		p2pMetroNodes.Add(oltNode.Get(0));
		p2pMetroDevices = pointToPoint.Install (p2pMetroNodes);
	//}

      
  /////////////////////////////////////////////////////ASSIGN IP ADDRESSES
  
  //Ip addresses for OLT and ONUs
  Ipv4AddressHelper addressHelper;
  Ptr<XgponOltNetDevice> tmpDevice = DynamicCast<XgponOltNetDevice, NetDevice> (xgponDevices.Get(0));
  std::string xgponIpbase = xgponHelper.GetXgponIpAddressBase ( );
  std::string xgponNetmask = xgponHelper.GetOltAddressNetmask();
  addressHelper.SetBase (xgponIpbase.c_str(), xgponNetmask.c_str());
  std::cout << "xgponIpbase:" << xgponIpbase << ", oltNetMask:" << xgponNetmask << std::endl;


  Ipv4InterfaceContainer xgponInterfaces = addressHelper.Assign (xgponDevices);
  for(int i=0; i<(nOnus+1);i++)
  {
    Ptr<XgponNetDevice> tmpDevice = DynamicCast<XgponNetDevice, NetDevice> (xgponDevices.Get(i));
    Ipv4Address addr = xgponInterfaces.GetAddress(i);
    tmpDevice->SetAddress (addr);
    if(i==0)
      {std::cout << "olt addr: " ; addr.Print(std::cout); std::cout << std::endl;}
    else
      {std::cout << "onu " << i-1 << " addr:" ; addr.Print(std::cout); std::cout << std::endl;}
  }
 
  //assign ip address for metro interfaces
  for(int i=0; i<nOnus; i++) {
		std::string metroIpbase = xgponHelper.GetIpAddressBase (172, i, 24);
	  std::string metroNetmask = xgponHelper.GetIpAddressNetmask (24); 
		addressHelper.SetBase (metroIpbase.c_str(), metroNetmask.c_str());
		p2pMetroInterfaces[i] = addressHelper.Assign (p2pMetroDevices);
	}

  //assign ip address for core, lastmile and edge interfaces
	//given multiple clients per ONU are possible and with each client having it's own dedicated server, setup a netmask such that each user-edge device and each server-router device gets ip address from a specific subnet; this subnet is reserved by the for MSB of the last byte of the IP address
	int netMaskFactor = 256/pow(2, ceil(log2(nFlwOnu))); //get the upper limit of no. of bits, as power of 2, needed to be reserved for the client subnets per Edge Node and per Router interface
	NS_ASSERT_MSG( ((netMaskFactor >=16) & (netMaskFactor %4 == 0)), "only the first for bits of the last byte to be allocated to the no. of clinets per ONU");
	int tmp = netMaskFactor;
	int netMaskSum = 0;
	do{
		netMaskSum += tmp;
		tmp *= 2;
	}
	while(tmp < 256);
	//std::cout << "test: log2(4): " << log2(4) << ", log2(15): " << log2(15) << ", ceil(log2(15)): " << ceil(log2(15)) << ", log2(16): " << log2(16) << std::endl;
	//core and edge interfaces
	std::string ipBaseCore, ipBaseEdge, netMask;
  for(int i=0; i<nOnus; i++) {
    for(int j=0; j<nFlwOnu; j++){
			ipBaseCore = "171.0." + std::to_string(i) + "." + std::to_string(j*netMaskFactor);
			ipBaseEdge = "174.0." + std::to_string(i) + "." + std::to_string(j*netMaskFactor);
			netMask = "255.255.255." + std::to_string(netMaskSum) ; 
			//std::cout << "routingDebug: ipBaseCore : " << ipBaseCore << ", ipBaseEdge: " << ipBaseEdge << ", netMask: " << netMask << ", netMaskFactor : " << netMaskFactor  << ", netMaskSum: " << netMaskSum << std::endl;
      k = i*nFlwOnu + j;
			
			addressHelper.SetBase (ipBaseCore.c_str(), netMask.c_str());
			p2pCoreInterfaces[k] = addressHelper.Assign (p2pCoreDevices[k]);
			//std::cout << "routingDebug: assigned core interfaces: serverInterface - " << p2pCoreInterfaces[k].GetAddress(0) << ", routerInterface - " << p2pCoreInterfaces[k].GetAddress(1) << std::endl;
      
			addressHelper.SetBase (ipBaseEdge.c_str(), netMask.c_str());
			p2pEdgeInterfaces[k] = addressHelper.Assign (p2pEdgeDevices[k]);
      //std::cout << "routingDebug: assigned edge interfaces: userInterface - " << p2pEdgeInterfaces[k].GetAddress(0) << ", edgeNodeInterface - " << p2pEdgeInterfaces[k].GetAddress(1) << std::endl;
		}
  }
  
  
  //lastmile interfaces
	for(int i=0; i<nOnus; i++) {
    Ptr<XgponOnuNetDevice> tmpDevice = DynamicCast<XgponOnuNetDevice, NetDevice> (xgponDevices.Get(i+1));
    std::string onuIpbase = xgponHelper.GetOnuIpAddressBase (tmpDevice);
    std::string onuNetmask = xgponHelper.GetOnuAddressNetmask();     
    addressHelper.SetBase (onuIpbase.c_str(), onuNetmask.c_str());
    p2pLastMileInterfaces[i] = addressHelper.Assign (p2pLastMileDevices[i]);
    //std::cout << "Assigned lastMile interfaces: " << p2pLastMileInterfaces[i].GetAddress(0) << ", " << p2pLastMileInterfaces[i].GetAddress(1) << std::endl;
  }


//////////////////////////////////////////////CONFIGURE THE XGPON WITH QOS CLASSES, UPSTREAM/DOWNSTREAM CONNECTIONS AND IP-ALLOCID-PORTID-TCONT MAPPINGS
  uint32_t min_queue_size; //unit: Bytes, set the min queue size required in the ONUs and OLTs as per the capacity of and direction of traffic in XGPON; based on the principle of 0.5 BDP when using the Bandwidth of XG(S) PON and the round-trip delay
  if (traffic_direction == "downstream"){
    min_queue_size = 0.5 * max_bandwidth * 1e9 * 0.005 / 8 ; //there's only one queue in the downstream, hence no impact by the no. of ONUs. But maximum expected queue dealy at OLT is 5ms. Any higher values of the queue size will have a higher maximum queue delay at OLT; max_bandwidth at Gbps, delay at ms and the resulting unit is Bytes
    std::cout << "queue size at OLT (and ONU) for a 5ms avg delay: " << min_queue_size*1e-3 << " KBytes" << std::endl;
  }else{
    min_queue_size = (upstream_dba == "RoundRobin") ? (0.5 * max_bandwidth * 1e9 * 0.005 / (nOnus * 8)) : (0.5 * max_bandwidth * 1e9 * 0.005 / (3 * nOnus * 8)) ; //min_queue_size is based on a maximum expected queue dealy at ONU of 5ms and 3 TCONTs per ONU; hence there's no division by 3 if the DBA is not RoundRobin
    std::cout << "queue size at ONU for a 5ms max delay: " << min_queue_size*1e-3 << " KBytes, with max_bandwdith = " << max_bandwidth << " and nOnus = " << nOnus << std::endl;
  }
  Config::SetDefault ("ns3::XgponQueue::MaxBytes", UintegerValue(min_queue_size)); //set the queue size. min_queue_size (unit: Bytes), MaxBytes (unit: Bytes)

  Ptr<XgponOltNetDevice> oltDevice = DynamicCast<XgponOltNetDevice, NetDevice> (xgponDevices.Get(0));
	if(traffic_direction == "upstream"){
    std::cout << "Connecting OLT to upstream statistics " << std::endl;
    oltDevice->TraceConnectWithoutContext ("DeviceStatistics", MakeCallback(&DeviceStatisticsTrace));
   }
	
	//add xgem ports for user nodes connected to ONUs
  //each onu-olt pair would have a single downstream port and multiple upstream ports depending on the no. of tconts (by default 4 tconts, hence 4 upstream ports)
  int nTconts = upstream_dba == "RoundRobin" ? 1 : 4; // if the DBA is RoundRobin, only one allocId (of tcontType 1) is suffient, ja:update:xgsponv3
  for(int i=0; i< nOnus; i++) 
  {

    Address addr = p2pLastMileInterfaces[i].GetAddress(1); //address of the ONU
    Ptr<XgponOnuNetDevice> onuDevice = DynamicCast<XgponOnuNetDevice, NetDevice> (xgponDevices.Get(i+1));
    if(traffic_direction == "downstream"){
      std::cout << "Connecting ONU " << i << " to device downstream statistics " << std::endl;
      onuDevice->TraceConnectWithoutContext ("DeviceStatistics", MakeCallback(&DeviceStatisticsTraceOnu));
    }
		
    uint16_t downPortId = xgponHelper.AddOneDownstreamConnectionForOnu (onuDevice, oltDevice, addr); 
    
    //tconts 1-4 are provisioned for the upstream connection (using allocId and upPortId)
		for (uint8_t tcont=1; tcont<= nTconts; tcont++){
      //these parameters are not in use when using the RoundRobin DBA
			xgponHelper.SetQosParametersAttribute ("FixedBandwidth", UintegerValue (fixedBw[i]) );
	    xgponHelper.SetQosParametersAttribute ("AssuredBandwidth", UintegerValue (assuredBw[i]) );
 		  xgponHelper.SetQosParametersAttribute ("NonAssuredBandwidth", UintegerValue(nonAssuredBw[i]) );
   		xgponHelper.SetQosParametersAttribute ("BestEffortBandwidth", UintegerValue (bestEffortBw[i]) );
	    xgponHelper.SetQosParametersAttribute ("MaxServiceInterval", UintegerValue (siMax[i]));
   		xgponHelper.SetQosParametersAttribute ("MinServiceInterval", UintegerValue (siMin[i]));
     	
			XgponQosParameters::XgponTcontType tcontType = static_cast<XgponQosParameters::XgponTcontType>(tcont);
     	uint16_t allocId = xgponHelper.AddOneTcontForOnu (onuDevice, oltDevice, tcontType); 
     	uint16_t upPortId = xgponHelper.AddOneUpstreamConnectionForOnu (onuDevice, oltDevice, allocId, addr);
      allocIdList.push_back(allocId);
 	    std::cout << "\t\tUP/DOWNSTREAM ONU-ID = " << onuDevice->GetOnuId() << "\tALLOC-ID = " << allocId << "\tUP-PORT-ID= " << upPortId << "\tDOWN-PORT-ID = " << downPortId << "\tTCONT-"   << tcontType << std::endl;
   	}
  }

  //POPULATE ROUTING TABLE
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  //PREPARE FOR CREATING TRAFFIC FLOWS WITH APPROPRIATE TCONT TYPES
  int appPort[nFlwOnu]; //a dedicated port for each traffic flow type
  int p = upstream_dba == "RoundRobin" ? nTconts : 1; // if the DBA is RoundRobin, only one type of traffic is suffient
  int flowsToTcont[nFlwOnu]; //create a mapping array and
  for (int j=0;j<nFlwOnu;j++){
		flowsToTcont[j] = p;
		std::cout << "in each onu, flow: " << j << " is set with ToS/priority " << flowsToTcont[j] << std::endl;
	} //manually map each flow type to tcont type; in RR, all flows are mapped to T1 (or a single Tcont Type)
    
  
////////////////////////////////////////////////////CONFIGURE APPLICATION TRAFFIC
	Config::SetDefault("ns3::TcpL4Protocol::SocketType",StringValue("ns3::TcpCubic")); 
	Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(tcp_segment_size));
	//Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
	//Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));
  
  if(traffic_direction == "upstream"){
		for (int i=0; i<nOnus; i++){
			for(int j =0; j<nFlwOnu ; j++){//no. of TCP traffic flows per ONU, with each traffic flow for each traffic type 
		    //AN EXAMPLE UDP APPLICATION FOR THE UPSTREAM DIRECTION (THE SAME SETTINGS, WITH THE SOURCE AND DESTIONATION SWAPPED, CAN BE USED, IF NEEDED, FOR A DOWNSTREAM UDP APPLICATION
				appPort[j] = 9000; //common port number for all TCP flows
				ApplicationContainer sinkApp, sourceApp;
        InetSocketAddress sinkAddr = InetSocketAddress(p2pCoreInterfaces[nFlwOnu*i+j].GetAddress(0), appPort[j]); //0 is the server end of the interface in p2pCoreInterfaces
          sinkAddr.SetTos(flowsToTcont[j] << 2); //last two bits of the ToS are used as ECN markers, hence if used, will be removed at the IP layer; so shifting the bits to the left to avoid being masked at the ECN.
        PacketSinkHelper sink ("ns3::UdpSocketFactory", Address(sinkAddr));
          sink.SetAttribute("Local", AddressValue(sinkAddr));
          sinkApp = sink.Install (serverNodes.Get(nFlwOnu*i+j));
          sinkApp.Start (Seconds (0.05));
          sinkApp.Stop (Seconds (APP_STOP));
        std::cout << "onu: " << i << ", UDP sink (server) node address: " << sinkAddr << ", id - " << serverNodes.Get(nFlwOnu*i+j)->GetId() << std::endl;

        InetSocketAddress sourceAddr = InetSocketAddress(p2pEdgeInterfaces[nFlwOnu*i+j].GetAddress(0), appPort[j]); //0 is the server end of the interface in p2pCoreInterfaces
          sourceAddr.SetTos(flowsToTcont[j] << 2); //last two bits of the ToS are used as ECN markers, hence if used, will be removed at the IP layer; so shifting the bits to the left to avoid being masked at the ECN.
          std::cout << "\tsetting tcontType: " << flowsToTcont[j]  << " for ONU: " << i << " with UDP flow " << j << std::endl;
        OnOffHelper onOff ("ns3::UdpSocketFactory", sourceAddr);
					onOff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
					onOff.SetAttribute ("OffTime",  StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
					onOff.SetAttribute ("DataRate", StringValue(per_app_rate));
					onOff.SetAttribute ("PacketSize", UintegerValue(1400)); //1436 is the maximum packet size at the application that avoids segmentation at the Framing Sub layer in XGSPON; for XGPON, this value is 1447
					onOff.SetAttribute ("MaxBytes", UintegerValue(0));
          onOff.SetAttribute("Remote", AddressValue(sinkAddr));
          onOff.SetAttribute("Local", AddressValue(sourceAddr));
          sourceApp = onOff.Install (userNodes.Get(nFlwOnu*i+j));
          sourceApp.Start (Seconds (0.5 + 0.001*i));
          sourceApp.Stop (Seconds (APP_STOP));
        std::cout << "onu:" << i << ", UDP source(user/edge) node address: " << sourceAddr <<
            ", id - " << userNodes.Get(nFlwOnu*i+j)->GetId() << ", destination address: " << sinkAddr << std::endl;
      }
		}
	}else if(traffic_direction == "downstream"){
		for (int i=0; i<nOnus; i++){
			for(int j =0; j<nFlwOnu ; j++){//no. of TCP traffic flows per ONU, with each traffic flow for each traffic type 
		    //AN EXAMPLE TCP APPLICATION FOR THE DOWNSTREAM DIRECTION (THE SAME SETTINGS, WITH THE SOURCE AND DESTIONATION SWAPPED, CAN BE USED, IF NEEDED, FOR AN UPSTREAM UDP APPLICATION
				appPort[j] = 9000; //common port number for all TCP flows 
				ApplicationContainer sinkApp, sourceApp;
				InetSocketAddress sinkAddr = InetSocketAddress(p2pEdgeInterfaces[nFlwOnu*i+j].GetAddress(0), appPort[j]); //0 is the server end of the interface in p2pCoreInterfaces
					sinkAddr.SetTos(flowsToTcont[j] << 2);
				PacketSinkHelper sink ("ns3::TcpSocketFactory", Address(sinkAddr));
					sink.SetAttribute("Local", AddressValue(sinkAddr));
					sinkApp = sink.Install (userNodes.Get(nFlwOnu*i+j));
					sinkApp.Start (Seconds (0.05));
					sinkApp.Stop (Seconds (APP_STOP + 0.5));
				std::cout << "onu: " << i << ", TCP sink (user/edge) node address: " << sinkAddr << ", id - " << userNodes.Get(nFlwOnu*i+j)->GetId() << std::endl;
				
				InetSocketAddress sourceAddr = InetSocketAddress(p2pCoreInterfaces[nFlwOnu*i+j].GetAddress(0), appPort[j]); //0 is the server end of the interface in p2pCoreInterfaces
					sourceAddr.SetTos(flowsToTcont[j] << 2);
					std::cout << "\tsetting tcontType: " << flowsToTcont[j]  << " for ONU: " << i << " with TCP flow " << j << std::endl;
				BulkSendHelper app ("ns3::TcpSocketFactory", sourceAddr);
					app.SetAttribute ("MaxBytes", UintegerValue(0));
					app.SetAttribute("Remote", AddressValue(sinkAddr));
					app.SetAttribute("Local", AddressValue(sourceAddr));
					sourceApp = app.Install (serverNodes.Get(nFlwOnu*i+j));
					sourceApp.Start (Seconds (0.1 + 1.0*j + 0.001*i));
		      sourceApp.Stop (Seconds (APP_STOP + 0.3));
				std::cout << "onu:" << i << ", TCP source(server) node address: " << sourceAddr << ", id - " << serverNodes.Get(nFlwOnu*i+j)->GetId() << ", destination address: " << sinkAddr << std::endl;
      }
		}
	}


	std::cout << "BulkHelper: " << std::endl;
	std::cout << "\tSocketType: TCP, SegmentSize: " << tcp_segment_size << std::endl;
	
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  //TRACING AND LOGGING
	//Simulator::Schedule(Seconds(0.5), &ConnectSocketTraces);
  
	//pointToPoint.EnablePcap("p2p-user-pcap", userNodes);
  //pointToPoint.EnablePcap("p2p-metro-pcap", p2pMetroNodes);
  //pointToPoint.EnablePcap("p2p-server-pcap", serverNodes);

  Simulator::Stop(Seconds(SIM_STOP));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;


}
