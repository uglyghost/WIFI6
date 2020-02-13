/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 IITP RAS
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
 * Author: Pavel Boyko <boyko@iitp.ru>
 *
 * Classical hidden terminal problem and its RTS/CTS solution.
 *
 * Topology: [node 0] <-- -50 dB --> [node 1] <-- -50 dB --> [node 2]
 *
 * This example illustrates the use of
 *  - Wifi in ad-hoc mode
 *  - Matrix propagation loss model
 *  - Use of OnOffApplication to generate CBR stream
 *  - IP flow monitor
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ssid.h"

using namespace ns3;

/// Run single 10 seconds experiment
void experiment ()
{

  double simulationTime = 10; //seconds
  int mcs = 1;
  int gi = 800;
  int channelWidth = 20;
  bool useExtendedBlockAck = false;
  uint16_t numberOfAPNodes = 1;
  uint16_t numberOfSTANodes = 6;
  // 0. Enable or disable CTS/RTS
  //UintegerValue ctsThr = (enableCtsRts ? UintegerValue (100) : UintegerValue (2200));
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);

  // 1. Create 3 nodes
  NodeContainer apnodes;
  apnodes.Create (numberOfAPNodes);
  NodeContainer stanodes;
  stanodes.Create (numberOfSTANodes);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=30]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apnodes);
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|200|0|200"));
  //mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(stanodes);

  // 3. Create propagation loss matrix
  Config::SetDefault ("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue (40.046));

  // 4. Create & setup wifi channel
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

  // 5. Install wireless devices
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_2_4GHZ);
  Config::SetDefault ("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue (40.046));
  //wifi.SetRemoteStationManager ("ns3::" + wifiManager + "WifiManager");
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;
  //wifiMac.SetType ("ns3::AdhocWifiMac"); // use ad-hoc MAC
  //NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  std::ostringstream oss;
  oss << "HeMcs" << mcs;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
                                "ControlMode", StringValue (oss.str ()));

  Ssid ssid = Ssid ("ns3-80211ax");

  wifiMac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (wifiPhy, wifiMac, stanodes);
  //staDevice = wifi.Install (wifiPhy, wifiMac, nodes.Get(1));

  wifiMac.SetType ("ns3::ApWifiMac",
               "EnableBeaconJitter", BooleanValue (true),
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (wifiPhy, wifiMac, apnodes);
  //apDevice = wifi.Install (wifiPhy, wifiMac, nodes.Get(1));
  // Set channel width, guard interval and MPDU buffer size
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/GuardInterval", TimeValue (NanoSeconds (gi)));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/MpduBufferSize", UintegerValue (useExtendedBlockAck ? 256 : 64));

  // uncomment the following to have athstats output
  // AthstatsHelper athstats;
  // athstats.EnableAthstats(enableCtsRts ? "rtscts-athstats-node" : "basic-athstats-node" , nodes);

  // uncomment the following to have pcap output
  // wifiPhy.EnablePcap (enableCtsRts ? "rtscts-pcap-node" : "basic-pcap-node" , nodes);


  // 6. Install TCP/IP stack & assign IP addresses
  InternetStackHelper internet;
  internet.Install (apnodes);
  internet.Install (stanodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = ipv4.Assign (apDevice);
  //Ipv4AddressHelper ipv4;
  //ipv4.SetBase ("10.0.1.0", "255.0.0.0");
  staInterfaces = ipv4.Assign (staDevice);

  // 7. Install applications: two CBR streams each saturating the channel
  ApplicationContainer cbrApps;
  uint16_t cbrPort = 12345;
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (apInterfaces.GetAddress (0), cbrPort));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (1400));
  onOffHelper.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // flow 1:  node 0 -> node 1
  onOffHelper.SetAttribute ("DataRate", StringValue ("3000000bps"));
  onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.000000)));
  cbrApps.Add (onOffHelper.Install (apnodes));

  // flow 2:  node 2 -> node 1
  /** \internal
   * The slightly different start times and data rates are a workaround
   * for \bugid{388} and \bugid{912}
   */
  onOffHelper.SetAttribute ("DataRate", StringValue ("3001100bps"));
  onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.001)));
  cbrApps.Add (onOffHelper.Install (stanodes));

  //onOffHelper.SetAttribute ("DataRate", StringValue ("3001200bps"));
  //onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.002)));
  //cbrApps.Add (onOffHelper.Install (apnodes));

  /** \internal
   * We also use separate UDP applications that will send a single
   * packet before the CBR flows start.
   * This is a workaround for the lack of perfect ARP, see \bugid{187}
   */
  uint16_t  echoPort = 9;
  UdpEchoClientHelper echoClientHelper (apInterfaces.GetAddress (0), echoPort);
  echoClientHelper.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClientHelper.SetAttribute ("PacketSize", UintegerValue (10));
  ApplicationContainer pingApps;

  // again using different start times to workaround Bug 388 and Bug 912
  echoClientHelper.SetAttribute ("StartTime", TimeValue (Seconds (0.001)));
  pingApps.Add (echoClientHelper.Install (stanodes));
  //echoClientHelper.SetAttribute ("StartTime", TimeValue (Seconds (0.006)));
  //pingApps.Add (echoClientHelper.Install (apnodes));

  // 8. Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 9. Run simulation for 10 seconds
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // 10. Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      // first 2 FlowIds are for ECHO apps, we don't want to display them
      //
      // Duration for throughput measurement is 9.0 seconds, since
      //   StartTime of the OnOffApplication is at about "second 1"
      // and
      //   Simulator::Stops at "second 10".
      if (i->first > 2)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
          if(i->second.rxPackets>0)
            {
              std::cout << "  Mean Delay:   " << (i->second.delaySum / i->second.rxPackets) << " \n";
              std::cout << "  Mean Jitter: " << (i->second.jitterSum / (i->second.rxPackets-1))  << " \n";
              std::cout << "  Mean Hop Count: " << float(i->second.timesForwarded) / i->second.rxPackets + 1  << " \n";
            }
        }
    }

  // 11. Cleanup
  Simulator::Destroy ();
}

int main (int argc, char **argv)
{
  //std::string wifiManager ("Arf");
  //CommandLine cmd;
  //cmd.AddValue ("wifiManager", "Set wifi rate manager (Aarf, Aarfcd, Amrr, Arf, Cara, Ideal, Minstrel, Onoe, Rraa)", wifiManager);
  //cmd.Parse (argc, argv);

  //std::cout << "Hidden station experiment with RTS/CTS disabled:\n" << std::flush;
  //experiment (false, wifiManager);
  //std::cout << "------------------------------------------------\n";
  //std::cout << "Hidden station experiment with RTS/CTS enabled:\n";
  experiment ();

  return 0;
}