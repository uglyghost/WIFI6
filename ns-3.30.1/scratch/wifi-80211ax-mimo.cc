/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Sébastien Deronne <sebastien.deronne@gmail.com>
 */

// This example is used to validate 802.11n MIMO.
//
// It outputs plots of the throughput versus the distance
// for every HT MCS value and from 1 to 4 MIMO streams.
//
// The simulation assumes a single station in an infrastructure network:
//
//  STA     AP
//    *     *
//    |     |
//   n1     n2
//
// The user can choose whether UDP or TCP should be used and can configure
// some 802.11n parameters (frequency, channel width and guard interval).


#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-loss-counter.h"
#include "ns3/yans-wifi-channel.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  std::vector <std::string> modes;
  modes.push_back ("HtMcs0");
  modes.push_back ("HtMcs1");
  modes.push_back ("HtMcs2");
  modes.push_back ("HtMcs3");
  modes.push_back ("HtMcs4");
  modes.push_back ("HtMcs5");
  modes.push_back ("HtMcs6");
  modes.push_back ("HtMcs7");
  modes.push_back ("HtMcs8");
  modes.push_back ("HtMcs9");
  modes.push_back ("HtMcs10");
  modes.push_back ("HtMcs11");
  modes.push_back ("HtMcs12");
  modes.push_back ("HtMcs13");
  modes.push_back ("HtMcs14");
  modes.push_back ("HtMcs15");
  
  uint32_t mcs_set[] = { 15 }; //
  double distance[] = { 10 };  //, 30, 60, 100
  uint16_t mimo[] = { 8 };  //, 2, 4
  uint32_t ChannelWidths[] = {20}; //, 80, 160
  bool udp = true;
  uint16_t numberOfAPNodes = 1;
  uint16_t numberOfSTANodes = 1;
  double simulationTime = 10; //seconds
  double frequency = 5.0; //whether 2.4 or 5.0 GHz
  double step = 10; //meters
  bool shortGuardInterval = false;
  std::string CSVfileName = "scratch/result.csv";

  CommandLine cmd;
  cmd.AddValue ("step", "Granularity of the results to be plotted in meters", step);
  cmd.AddValue ("simulationTime", "Simulation time per step (in seconds)", simulationTime);
  cmd.AddValue ("shortGuardInterval", "Enable/disable short guard interval", shortGuardInterval);
  cmd.AddValue ("frequency", "Whether working in the 2.4 or 5.0 GHz band (other values gets rejected)", frequency);
  cmd.AddValue ("udp", "UDP if set to 1, TCP otherwise", udp);
  cmd.Parse (argc,argv);


  for (uint32_t MCS_index = 0; MCS_index < (sizeof(mcs_set)/sizeof(mcs_set[0])); MCS_index++) //MCS
    {
      std::cout << "HtMCS"<<mcs_set[MCS_index] << std::endl;
	  
	  if(MCS_index==0){//clean csv file when start
		  std::ofstream of_clear (CSVfileName.c_str(), std::ios::out | std::ios::trunc);
		  of_clear << "MCS""," << "Distance/m""," << "MIMO""," << "ChannelWidth/MHz"",";
		  of_clear << "Throughput/Mbps"","<<"PacketLoss/%"","<<"Mean Delay/ns"","<<"Mean Jitter/ns""\n";
		  of_clear.close ();
	  }
	  
      for (uint32_t d_index = 0; d_index < (sizeof(distance)/sizeof(distance[0])); d_index++) //distance
        {
		  double d = distance[d_index];
          std::cout << "Distance = " << d << "m: " << std::endl;
          
		  for(uint32_t mimo_index = 0; mimo_index < (sizeof(mimo)/sizeof(mimo[0])); mimo_index++)
		  {
			uint16_t Antennas = mimo[mimo_index];
			std::cout << "MIMO = "  << Antennas << std::endl;
			
			for(uint32_t width_index = 0; width_index < (sizeof(ChannelWidths)/sizeof(ChannelWidths[0])); width_index++)
			{
				uint32_t ChannelWidth = ChannelWidths[width_index];
				std::cout << "ChannelWidth = "  << ChannelWidth << std::endl;
				
				uint32_t payloadSize = 1472; //1500 byte IP packet
		  
				std::ostringstream oss;
				oss << "HtMcs" << mcs_set[MCS_index];
				
				uint8_t nStreams = 1 + (mcs_set[MCS_index] / 8); //number of MIMO streams
		
				NodeContainer wifiStaNode;
				wifiStaNode.Create (numberOfSTANodes);
				NodeContainer wifiApNode;
				wifiApNode.Create (numberOfAPNodes);
		
				YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
				YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
				phy.SetChannel (channel.Create ());
		
				// Set MIMO capabilities
				// stream < Antennas
				if(nStreams > Antennas)
				{
					continue;
				}
				phy.Set ("Antennas", UintegerValue (Antennas));
				phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (nStreams));
				phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (nStreams));
		
				WifiMacHelper mac;
				WifiHelper wifi;
				if (frequency == 5.0)
					{
					wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_5GHZ );
					//Config::SetDefault ("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue (40.046));
					}
					
				else
					{
					std::cout << "Wrong frequency value!" << std::endl;
					return 0;
					}
		
				StringValue ctrlRate;
				ctrlRate = StringValue ("OfdmRate24Mbps");
					
				wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
												"DataMode", StringValue (oss.str ()),
												"ControlMode", ctrlRate);
		
				Ssid ssid = Ssid ("ns3-80211ax");
		
				mac.SetType ("ns3::StaWifiMac",
							"Ssid", SsidValue (ssid));
		
				NetDeviceContainer staDevice;
				staDevice = wifi.Install (phy, mac, wifiStaNode);
		
				mac.SetType ("ns3::ApWifiMac",
							"Ssid", SsidValue (ssid));
		
				NetDeviceContainer apDevice;
				apDevice = wifi.Install (phy, mac, wifiApNode);
		
				// Set channel width
				Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (ChannelWidth));
		
				// Set guard interval
				Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (shortGuardInterval));
		
				// mobility.
				MobilityHelper mobility;
				Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
		
				positionAlloc->Add (Vector (0.0, 0.0, 0.0));
				positionAlloc->Add (Vector (d, 0.0, 0.0));
				mobility.SetPositionAllocator (positionAlloc);
		
				mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
		
				mobility.Install (wifiApNode);
				mobility.Install (wifiStaNode);
		
				/* Internet stack*/
				InternetStackHelper stack;
				stack.Install (wifiApNode);
				stack.Install (wifiStaNode);
		
				Ipv4AddressHelper address;
				address.SetBase ("192.168.1.0", "255.255.255.0");
				Ipv4InterfaceContainer staNodeInterface;
				Ipv4InterfaceContainer apNodeInterface;
		
				staNodeInterface = address.Assign (staDevice);
				apNodeInterface = address.Assign (apDevice);
		
				/* Setting applications */
				ApplicationContainer serverApp;
				if (udp)
					{
					//UDP flow
					uint16_t port = 9;
					UdpServerHelper server (port);
					serverApp = server.Install (wifiStaNode.Get (0));
					serverApp.Start (Seconds (0.0));
					serverApp.Stop (Seconds (simulationTime + 1));
		
					UdpClientHelper client (staNodeInterface.GetAddress (0), port);
					client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
					client.SetAttribute ("Interval", TimeValue (Time ("0.00001"))); //packets/s
					client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
					ApplicationContainer clientApp = client.Install (wifiApNode.Get (0));
					clientApp.Start (Seconds (1.0));
					clientApp.Stop (Seconds (simulationTime + 1));
					}
				else
					{
					//TCP flow
					uint16_t port = 50000;
					Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
					PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", localAddress);
					serverApp = packetSinkHelper.Install (wifiStaNode.Get (0));
					serverApp.Start (Seconds (0.0));
					serverApp.Stop (Seconds (simulationTime + 1));
		
					OnOffHelper onoff ("ns3::TcpSocketFactory",Ipv4Address::GetAny ());
					onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
					onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
					onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
					onoff.SetAttribute ("DataRate", DataRateValue (1000000000)); //bit/s
					AddressValue remoteAddress (InetSocketAddress (staNodeInterface.GetAddress (0), port));
					onoff.SetAttribute ("Remote", remoteAddress);
					ApplicationContainer clientApp = onoff.Install (wifiApNode.Get (0));
					clientApp.Start (Seconds (1.0));
					clientApp.Stop (Seconds (simulationTime + 1));
					}
		
				
				
				Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
				
				
				//Install FlowMonitor on all nodes
				FlowMonitorHelper flowmon;
				Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
		
				Simulator::Stop (Seconds (simulationTime + 1));
				Simulator::Run ();
				
				// write into file
				std::ofstream out (CSVfileName.c_str (), std::ios::app);
				
				
				//Print per flow statistics
				monitor->CheckForLostPackets ();
				Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
				FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
				
				for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
				{
					Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
					std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
					std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
					std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
					std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";
					std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
					std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
					std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";
					//(i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000 / 1000  << " Mbps\n";
					std::cout << "  PacketLoss: " << (1-(i->second.lostPackets*1.0 / i->second.txPackets))*100 << " \n";
					if(i->second.rxPackets>0)
						{
						std::cout << "  Mean Delay:   " << i->second.delaySum / i->second.rxPackets << " \n";
						std::cout << "  Mean Jitter: " << (i->second.jitterSum / (i->second.rxPackets-1))  << " \n";
						std::cout << "  Mean Hop Count: " << float(i->second.timesForwarded) / i->second.rxPackets + 1  << " \n";
						}
					out << modes[MCS_index] <<","; //MCS
					out << d << "," << Antennas<<","<< ChannelWidth<<",";				// distance, antennas, channelwidth
					out << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000<<","; //throughput
					out << (100-(i->second.lostPackets*1.0 / i->second.txPackets)*100)<<",";    //packets loss
					//out << i->second.delaySum / i->second.rxPackets<<",";       		// Mean Delay
					out << i->second.delaySum.GetSeconds()*1000*1000 / i->second.rxPackets<<",";       // Mean Delay
					out << i->second.jitterSum / (i->second.rxPackets-1)<<"\n";   //Mean Jitter
					
				}
				out.close ();
				Simulator::Destroy ();
			}
		  }
		  
        }
  
    }
/*
  plot.SetTerminal ("postscript eps color enh \"Times-BoldItalic\"");
  plot.SetLegend ("Distance (Meters)", "Throughput (Mbit/s)");
  plot.SetExtra  ("set xrange [0:100]\n\
set yrange [0:600]\n\
set ytics 0,50,600\n\
set style line 1 dashtype 1 linewidth 5\n\
set style line 2 dashtype 1 linewidth 5\n\
set style line 3 dashtype 1 linewidth 5\n\
set style line 4 dashtype 1 linewidth 5\n\
set style line 5 dashtype 1 linewidth 5\n\
set style line 6 dashtype 1 linewidth 5\n\
set style line 7 dashtype 1 linewidth 5\n\
set style line 8 dashtype 1 linewidth 5\n\
set style line 9 dashtype 2 linewidth 5\n\
set style line 10 dashtype 2 linewidth 5\n\
set style line 11 dashtype 2 linewidth 5\n\
set style line 12 dashtype 2 linewidth 5\n\
set style line 13 dashtype 2 linewidth 5\n\
set style line 14 dashtype 2 linewidth 5\n\
set style line 15 dashtype 2 linewidth 5\n\
set style line 16 dashtype 2 linewidth 5\n\
set style line 17 dashtype 3 linewidth 5\n\
set style line 18 dashtype 3 linewidth 5\n\
set style line 19 dashtype 3 linewidth 5\n\
set style line 20 dashtype 3 linewidth 5\n\
set style line 21 dashtype 3 linewidth 5\n\
set style line 22 dashtype 3 linewidth 5\n\
set style line 23 dashtype 3 linewidth 5\n\
set style line 24 dashtype 3 linewidth 5\n\
set style line 25 dashtype 4 linewidth 5\n\
set style line 26 dashtype 4 linewidth 5\n\
set style line 27 dashtype 4 linewidth 5\n\
set style line 28 dashtype 4 linewidth 5\n\
set style line 29 dashtype 4 linewidth 5\n\
set style line 30 dashtype 4 linewidth 5\n\
set style line 31 dashtype 4 linewidth 5\n\
set style line 32 dashtype 4 linewidth 5\n\
set style increment user"                                                                                                                                                                                                                                                                                                                                   );
  plot.GenerateOutput (file);
  file.close ();
*/
  return 0;
}

