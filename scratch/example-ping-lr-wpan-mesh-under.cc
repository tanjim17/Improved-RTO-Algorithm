#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char** argv)
{
  uint32_t nWsnNodes = 4;
  bool verbose = false;
  uint maxRange = 90;

  Packet::EnablePrinting();

  CommandLine cmd(__FILE__);
  cmd.AddValue("nWsnNodes", "Number of nodes", nWsnNodes);
  cmd.AddValue("verbose", "turn on log components", verbose);
  cmd.AddValue("maxRange", "Max coverage range", maxRange);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    // LogComponentEnable ("Ping6Application", LOG_LEVEL_ALL);
    // LogComponentEnable ("LrWpanMac", LOG_LEVEL_ALL);
    // LogComponentEnable ("LrWpanPhy", LOG_LEVEL_ALL);
    // LogComponentEnable ("LrWpanNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_ALL);
  }
  // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
  // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);

  NodeContainer wsnNodes;
  wsnNodes.Create(nWsnNodes);

  // NodeContainer wiredNodes;
  // wiredNodes.Create(1);
  // wiredNodes.Add(wsnNodes.Get(0));

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    "MinX", DoubleValue(0.0),
    "MinY", DoubleValue(0.0),
    "DeltaX", DoubleValue(80),
    "DeltaY", DoubleValue(80),
    "GridWidth", UintegerValue(10),
    "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wsnNodes);

  // creating a channel with range propagation loss model  
  Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(maxRange));
  Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
  Ptr<RangePropagationLossModel> propModel = CreateObject<RangePropagationLossModel>();
  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
  channel->AddPropagationLossModel(propModel);
  channel->SetPropagationDelayModel(delayModel);

  LrWpanHelper lrWpanHelper;
  // setting the channel in helper
  lrWpanHelper.SetChannel(channel);
  // Add and install the LrWpanNetDevice for each node
  NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(wsnNodes);

  // Fake PAN association and short address assignment.
  // This is needed because the lr-wpan module does not provide (yet)
  // a full PAN association procedure.
  lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

  InternetStackHelper internetv6;
  internetv6.Install(wsnNodes);
  // internetv6.Install(wiredNodes.Get(0));

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install(lrwpanDevices);

  // CsmaHelper csmaHelper;
  // NetDeviceContainer csmaDevices = csmaHelper.Install(wiredNodes);

  Ipv6AddressHelper ipv6;
  // ipv6.SetBase(Ipv6Address("2001:cafe::"), Ipv6Prefix(64));
  // Ipv6InterfaceContainer wiredDeviceInterfaces;
  // wiredDeviceInterfaces = ipv6.Assign(csmaDevices);
  // wiredDeviceInterfaces.SetForwarding(1, true);
  // wiredDeviceInterfaces.SetDefaultRouteInAllNodes(1);

  ipv6.SetBase(Ipv6Address("2001:f00d::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer wsnDeviceInterfaces;
  wsnDeviceInterfaces = ipv6.Assign(sixLowPanDevices);
  wsnDeviceInterfaces.SetForwarding(0, true);
  wsnDeviceInterfaces.SetDefaultRouteInAllNodes(0);

  for (uint32_t i = 0; i < sixLowPanDevices.GetN(); i++)
  {
    Ptr<NetDevice> dev = sixLowPanDevices.Get(i);
    dev->SetAttribute("UseMeshUnder", BooleanValue(true));
    dev->SetAttribute("MeshUnderRadius", UintegerValue(10));
  }

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(wsnNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  for (uint i = 1; i < nWsnNodes; i++) {
    UdpEchoClientHelper echoClient(wsnDeviceInterfaces.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(500));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1 / (double)100)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wsnNodes.Get(i));
    clientApps.Start(Seconds(2.0 + i * 0.1));
    clientApps.Stop(Seconds(10.0 + i * 0.1));
  }

  // uint32_t packetSize = 10;
  // uint32_t maxPacketCount = 5;
  // Time interPacketInterval = Seconds(1.);
  // Ping6Helper ping6;

  // ping6.SetLocal(wsnDeviceInterfaces.GetAddress(nWsnNodes - 1, 1));
  // ping6.SetRemote(wiredDeviceInterfaces.GetAddress(0, 1));

  // ping6.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
  // ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
  // ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
  // ApplicationContainer apps = ping6.Install(wsnNodes.Get(nWsnNodes - 1));

  // apps.Start(Seconds(1.0));
  // apps.Stop(Seconds(10.0));

  // AsciiTraceHelper ascii;
  // lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("Ping-6LoW-lr-wpan-meshunder-lr-wpan.tr"));
  // lrWpanHelper.EnablePcapAll(std::string("Ping-6LoW-lr-wpan-meshunder-lr-wpan"), true);

  // csmaHelper.EnableAsciiAll(ascii.CreateFileStream("Ping-6LoW-lr-wpan-meshunder-csma.tr"));
  // csmaHelper.EnablePcapAll(std::string("Ping-6LoW-lr-wpan-meshunder-csma"), true);

  Simulator::Stop(Seconds(10+ nWsnNodes * 0.1));

  // Flow monitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> monitor;
  bool flow_monitor = true;
  if (flow_monitor) {
    monitor = flowHelper.InstallAll();
  }

  Simulator::Run();

  if (flow_monitor) {
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); iter++) {
      NS_LOG_UNCOND("Flow ID:" << iter->first);
      NS_LOG_UNCOND("Sent packets = " << iter->second.txPackets);
      NS_LOG_UNCOND("Received packets = " << iter->second.rxPackets);
      NS_LOG_UNCOND("Dropped packets = " << iter->second.txPackets - iter->second.rxPackets);
      NS_LOG_UNCOND("Packet drop ratio = " << (iter->second.txPackets - iter->second.rxPackets) * 100.0 / iter->second.txPackets << "%");
      NS_LOG_UNCOND("Delay = " << iter->second.delaySum);
      NS_LOG_UNCOND("Throughput = " << iter->second.rxBytes /
        (1000 * (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds())) << " kbps\n");
    }
  }

  Simulator::Destroy();

}

