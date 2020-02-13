/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Jaume Nin <jaume.nin@cttc.cat>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
//#include "ns3/gtk-config-store.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several AP,
 * attaches one STA per AP starts a flow for each STA to  and from a remote host.
 * It also  starts yet another flow between each STA pair.
 */

int
main (int argc, char *argv[])
{

  uint16_t numberOfAPNodes = 1;
  uint16_t numberOfSTANodes = 4;
  int simTime = 10;
  Time interPacketInterval = MilliSeconds (100);
  bool useCa = false;
  bool disableDl = true;
  bool disableUl = true;
  bool disablePl = true;
  int mcs = 1;
  int gi = 800;

  // Argument about wifi-6
  double frequency = 5.0; //whether 2.4 or 5.0 GHz
  bool useExtendedBlockAck = false;
  double minExpectedThroughput = 0;
  double maxExpectedThroughput = 0;
  int channelWidth = 20;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("numberOfAPNodes", "Number of AP pairs", numberOfAPNodes);
  cmd.AddValue ("numberOfSTANodes", "Number of UE pairs", numberOfSTANodes);
  cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
  cmd.AddValue ("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.AddValue ("disableDl", "Disable downlink data flows", disableDl);
  cmd.AddValue ("disableUl", "Disable uplink data flows", disableUl);
  cmd.AddValue ("disablePl", "Disable data flows between peer UEs", disablePl);

  // Arguments about wifi-6
  cmd.AddValue ("frequency", "Whether working in the 2.4 or 5.0 GHz band (other values gets rejected)", frequency);
  cmd.AddValue ("useExtendedBlockAck", "Enable/disable use of extended BACK", useExtendedBlockAck);
  cmd.AddValue ("minExpectedThroughput", "if set, simulation fails if the lowest throughput is below this value", minExpectedThroughput);
  cmd.AddValue ("maxExpectedThroughput", "if set, simulation fails if the highest throughput is above this value", maxExpectedThroughput);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode;
  wifiStaNode.Create (numberOfSTANodes);
  NodeContainer wifiApNode;
  wifiApNode.Create (numberOfAPNodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  uint32_t payloadSize; //1500 byte IP packet
  payloadSize = 1472; //bytes
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  WifiMacHelper mac;
  WifiHelper wifi;
  if (frequency == 5.0)
    {
      wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_5GHZ);
    }
  else if (frequency == 2.4)
    {
      wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_2_4GHZ);
      Config::SetDefault ("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue (40.046));
    }
  else
    {
      std::cout << "Wrong frequency value!" << std::endl;
      return 0;
    }

  std::ostringstream oss;
  oss << "HeMcs" << mcs;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
                                "ControlMode", StringValue (oss.str ()));

  Ssid ssid = Ssid ("ns3-80211ax");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "EnableBeaconJitter", BooleanValue (false),
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode.Get(0));

  // Set channel width, guard interval and MPDU buffer size
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/GuardInterval", TimeValue (NanoSeconds (gi)));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/MpduBufferSize", UintegerValue (useExtendedBlockAck ? 256 : 64));


  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer p2pDevices = p2ph.Install (wifiApNode.Get(0), remoteHost);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=30]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|200|0|200"));
  //mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(wifiStaNode);
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=10]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(remoteHostContainer);


  // Install the IP stack on the STA
  InternetStackHelper stack;
  stack.Install (remoteHostContainer);
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);
  Ipv4Address remoteHostAddr = p2pInterfaces.GetAddress (1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevice);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevice);


  // Install and start applications on STAs and remote host
  uint16_t dlPort = 1100;
  uint16_t ulPort = 2000;
  uint16_t otherPort = 3000;
  ApplicationContainer dlserverApps;
  ApplicationContainer ulserverApps;
  ApplicationContainer serverApps;
  for (uint32_t u = 0; u < wifiStaNode.GetN (); ++u)
    {
      if (!disableDl)
        {
          UdpServerHelper dlserver (dlPort);
          dlserverApps = dlserver.Install (wifiStaNode.Get(u));
          dlserverApps.Start (Seconds (0.5));
          dlserverApps.Stop (Seconds (simTime + 1));

          UdpClientHelper dlClient (staInterfaces.GetAddress (u), dlPort);
          dlClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
          dlClient.SetAttribute ("Interval", TimeValue (Time ("0.00001"))); //packets/s
          dlClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          ApplicationContainer dlclientApps = dlClient.Install (remoteHost);
          dlclientApps.Start (Seconds (1.0));
          dlclientApps.Stop (Seconds (simTime + 1));
        }

      if (!disableUl)
        {
          ++ulPort;
          UdpServerHelper ulserver (ulPort);
          ulserverApps = ulserver.Install (remoteHost);
          ulserverApps.Start (Seconds (0.5));
          ulserverApps.Stop (Seconds (simTime + 1));

          UdpClientHelper ulClient (remoteHostAddr, ulPort);
          ulClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
          ulClient.SetAttribute ("Interval", TimeValue (Time ("0.00001"))); //packets/s
          ulClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          ApplicationContainer ulclientApps = ulClient.Install (wifiStaNode.Get(u));
          ulclientApps.Start (Seconds (1.0));
          ulclientApps.Stop (Seconds (simTime + 1));
        }

      if (!disablePl && numberOfSTANodes > 1)
        {
          ++otherPort;
          UdpServerHelper server (otherPort);
          serverApps = server.Install (wifiStaNode.Get(u));
          serverApps.Start (Seconds (0.5));
          serverApps.Stop (Seconds (simTime + 1));

          UdpClientHelper Client (staInterfaces.GetAddress (u), otherPort);
          Client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
          Client.SetAttribute ("Interval", TimeValue (Time ("0.00001"))); //packets/s
          Client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          ApplicationContainer clientApps = Client.Install (wifiStaNode.Get(u));
          clientApps.Start (Seconds (1.0));
          clientApps.Stop (Seconds (simTime + 1));
        }
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Flow monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;

  Simulator::Stop (Seconds(simTime));
  Simulator::Run ();

  // 10. Print per flow statistics
  flowMonitor->SerializeToXmlFile("WIFI6-FLOW.xml", true, true);
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();
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
              std::cout << "  Mean Delay:   " << (i->second.delaySum / i->second.rxPackets) << " s\n";
              std::cout << "  Mean Jitter: " << (i->second.jitterSum / (i->second.rxPackets-1))  << " \n";
              std::cout << "  Mean Hop Count: " << float(i->second.timesForwarded) / i->second.rxPackets + 1  << " \n";
            }
        }
    }

  return 0;
}