#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;



int main(int argc, char** argv)
{
    bool verbose = false;
    bool disablePcap = false;
    bool disableAsciiTrace = false;
    bool enableLSixlowLogLevelInfo = false;
    uint32_t maxRange = 1000;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.AddValue("disable-pcap", "disable PCAP generation", disablePcap);
    cmd.AddValue("disable-asciitrace", "disable ascii trace generation", disableAsciiTrace);
    cmd.AddValue("enable-sixlowpan-loginfo", "enable sixlowpan LOG_LEVEL_INFO (used for tests)", enableLSixlowLogLevelInfo);
    cmd.AddValue("maxRange", "Max coverage range", maxRange);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
        // LogComponentEnable("Ping6Application", LOG_LEVEL_ALL);
        // LogComponentEnable("LrWpanMac", LOG_LEVEL_ALL);
        // LogComponentEnable("LrWpanPhy", LOG_LEVEL_ALL);
        // LogComponentEnable("LrWpanNetDevice", LOG_LEVEL_ALL);
        // LogComponentEnable("SixLowPanNetDevice", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(20),
        "DeltaY", DoubleValue(20),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

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

    //LrWpanHelper lrWpanHelper;
    // Add and install the LrWpanNetDevice for each node
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // Fake PAN association and short address assignment.
    // This is needed because the lr-wpan module does not provide (yet)
    // a full PAN association procedure.
    lrWpanHelper.AssociateToPan(lrwpanDevices, 1);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces;
    deviceInterfaces = ipv6.Assign(devices);

    if (enableLSixlowLogLevelInfo)
    {
        std::cout << "Device 0: pseudo-Mac-48 " << Mac48Address::ConvertFrom(devices.Get(0)->GetAddress())
            << ", IPv6 Address " << deviceInterfaces.GetAddress(0, 1) << std::endl;
        std::cout << "Device 1: pseudo-Mac-48 " << Mac48Address::ConvertFrom(devices.Get(1)->GetAddress())
            << ", IPv6 Address " << deviceInterfaces.GetAddress(1, 1) << std::endl;
    }

    //   uint32_t packetSize = 10;
    //   uint32_t maxPacketCount = 5;
    //   Time interPacketInterval = Seconds (1.);
    //   Ping6Helper ping6;

    //   ping6.SetLocal (deviceInterfaces.GetAddress (0, 1));
    //   ping6.SetRemote (deviceInterfaces.GetAddress (1, 1));

    //   ping6.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    //   ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
    //   ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
    //   ApplicationContainer apps = ping6.Install (nodes.Get (0));

    //   apps.Start (Seconds (1.0));
    //   apps.Stop (Seconds (10.0));

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(deviceInterfaces.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(500));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1 / (double)100)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // if (!disableAsciiTrace)
    // {
    //     AsciiTraceHelper ascii;
    //     lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("Ping-6LoW-lr-wpan.tr"));
    // }
    // if (!disablePcap)
    // {
    //     lrWpanHelper.EnablePcapAll(std::string("Ping-6LoW-lr-wpan"), true);
    // }
    // if (enableLSixlowLogLevelInfo)
    // {
    //     Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    //     Ipv6RoutingHelper::PrintNeighborCacheAllAt(Seconds(9), routingStream);
    // }

    Simulator::Stop(Seconds(10));

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

