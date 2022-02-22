#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/wifi-net-device.h"
#include "ns3/ap-wifi-mac.h"
// #include "ns3/single-model-spectrum-channel.h"
#include <fstream>

// Default Network Topology
//
//   Wifi 10.1.2.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   *    *    *    *
//                                  AP
//                                     Wifi 10.1.3.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessDemo");

void writeToFile(std::ofstream& stream, int parameter, double value, std::string fileName) {
  stream.open(fileName + ".data", std::ios_base::app);
  stream << parameter << " " << value << std::endl;
  stream.close();
}

int
main(int argc, char* argv[])
{
  bool verbose = false;
  uint32_t nNodes = 5;
  uint32_t nFlows = 5;
  uint32_t nPacketsPerSec = 100;
  uint32_t maxRange = 1;
  bool tracing = false;
  bool flow_monitor = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nNodes", "Number of wifi STA devices on each side", nNodes);
  cmd.AddValue("nFlows", "Number of flows", nFlows);
  cmd.AddValue("nPacketsPerSec", "Number of packets per second", nPacketsPerSec);
  cmd.AddValue("maxRange", "Max coverage range", maxRange);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("flow_monitor", "Enable flow monitor", flow_monitor);

  cmd.Parse(argc, argv);

  // The underlying restriction of 18 is due to the grid position
  // allocator's configuration; the grid layout will exceed the
  // bounding box if more than 18 nodes are provided.
  // if (nNodes > 18)
  // {
  //   std::cout << "nNodes should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
  //   return 1;
  // }

  if (verbose)
  {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
  }

  // Create sources, sinks and access points
  NodeContainer sourceNodes;
  sourceNodes.Create(nNodes);
  NodeContainer sinkNodes;
  sinkNodes.Create(nNodes);
  NodeContainer apNodes;
  apNodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install(apNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    "MinX", DoubleValue(0.0),
    "MinY", DoubleValue(0.0),
    "DeltaX", DoubleValue(0.5),
    "DeltaY", DoubleValue(0.10),
    "GridWidth", UintegerValue(3),
    "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sourceNodes);
  mobility.Install(sinkNodes);
  mobility.Install(apNodes);

  YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
  YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
  Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(maxRange));
  channel1.AddPropagationLoss("ns3::RangePropagationLossModel");
  // channel2.AddPropagationLoss("ns3::RangePropagationLossModel");
  YansWifiPhyHelper phy1, phy2;
  phy1.SetChannel(channel1.Create());
  phy2.SetChannel(channel2.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NetDeviceContainer sourceDevices, sinkDevices, apDevices;
  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ns-3-ssid1");
  Ssid ssid2 = Ssid("ns-3-ssid2");

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
  sourceDevices = wifi.Install(phy1, mac, sourceNodes);
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
  apDevices = wifi.Install(phy1, mac, apNodes.Get(0));

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
  sinkDevices = wifi.Install(phy2, mac, sinkNodes);
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
  apDevices.Add(wifi.Install(phy2, mac, apNodes.Get(1)));

  // // set error model
  // Ptr<ReceiveListErrorModel> apPem = CreateObject<ReceiveListErrorModel> ();
  // apPem->SetList ({1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20});
  // Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice> (apDevices.Get (0));
  // dev->GetMac ()->GetWifiPhy ()->SetPostReceptionErrorModel (apPem);

  // Ptr<ReceiveListErrorModel> sta2Pem = CreateObject<ReceiveListErrorModel> ();
  // sta2Pem->SetList ({1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20});
  // dev = DynamicCast<WifiNetDevice> (sourceDevices.Get (0));
  // dev->GetMac ()->GetWifiPhy ()->SetPostReceptionErrorModel (sta2Pem);

  InternetStackHelper stack;
  stack.Install(sourceNodes);
  stack.Install(sinkNodes);
  stack.Install(apNodes);
  Ipv4AddressHelper address;

  address.SetBase("10.1.1.0", "255.255.255.0");
  address.Assign(p2pDevices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  address.Assign(sourceDevices);
  address.Assign(apDevices.Get(0));

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer sinkInterfaces;
  sinkInterfaces = address.Assign(sinkDevices);
  address.Assign(apDevices.Get(1));

  for (uint i = 0; i < nFlows; i++) {
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(sinkNodes.Get(i));
    serverApps.Start(Seconds(1.0 + i * 0.1));
    serverApps.Stop(Seconds(12.0 + i * 0.1));

    UdpEchoClientHelper echoClient(sinkInterfaces.GetAddress(i), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1 / (double)nPacketsPerSec)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(sourceNodes.Get(i));
    clientApps.Start(Seconds(2.0 + i * 0.1));
    clientApps.Stop(Seconds(12.0 + i * 0.1));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(12.0 + nFlows * 0.1));

  // AsciiTraceHelper ascii;
  // pointToPoint.EnableAsciiAll(ascii.CreateFileStream("third.tr"));

  // if (tracing)
  // {
  //   phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  //   pointToPoint.EnablePcapAll ("third");
  //   phy.EnablePcap("third", apDevices.Get(0));
  // }

  // Flow monitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> monitor;
  if (flow_monitor) {
    monitor = flowHelper.InstallAll();
  }

  Simulator::Run();

  // metric calculation
  double throughput = 0;
  double endToEndDelay = 0;
  int nPacketsSent = 0;
  int nPacketsReceived = 0;
  int nPacketsDropped = 0;
  int flowCount = 0;
  if (flow_monitor) {
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); iter++) {
      flowCount++;
      if (flowCount % 2) {
        throughput += iter->second.rxBytes / (1000 * (iter->second.timeLastRxPacket.GetSeconds()
          - iter->second.timeFirstTxPacket.GetSeconds()));
        endToEndDelay += iter->second.delaySum.GetDouble();
        nPacketsSent += iter->second.txPackets;
        nPacketsReceived += iter->second.rxPackets;
        nPacketsDropped += iter->second.lostPackets;

        NS_LOG_UNCOND("Flow ID:" << iter->first);
        NS_LOG_UNCOND("Sent packets = " << iter->second.txPackets);
        NS_LOG_UNCOND("Received packets = " << iter->second.rxPackets);
        NS_LOG_UNCOND("Dropped packets = " << iter->second.lostPackets);
        NS_LOG_UNCOND("Packet drop ratio = " << (iter->second.lostPackets) * 100.0 / iter->second.txPackets << "%");
        NS_LOG_UNCOND("Delay = " << iter->second.delaySum);
        NS_LOG_UNCOND("Throughput = " << iter->second.rxBytes /
          (1000 * (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds())) << " kbps\n");
      }
    }
  }

  std::ofstream througputStream, delayStream, deliveryStream, dropStream;
  writeToFile(througputStream, nNodes, throughput / nNodes, "throughput_nodes");
  writeToFile(delayStream, nNodes, endToEndDelay / nNodes, "delay_nodes");
  writeToFile(deliveryStream, nNodes, nPacketsReceived * 100 / (double)nPacketsSent, "delivery_nodes");
  writeToFile(dropStream, nNodes, nPacketsDropped * 100 / (double)nPacketsSent, "drop_nodes");

  Simulator::Destroy();
  return 0;
}