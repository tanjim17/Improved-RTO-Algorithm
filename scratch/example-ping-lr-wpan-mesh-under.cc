#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

double duration = 10;

class MyApp : public Application
{
public:

  MyApp();
  virtual ~MyApp();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleTx(void);
  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp()
  : m_socket(0),
  m_peer(),
  m_packetSize(0),
  m_dataRate(0),
  m_sendEvent(),
  m_running(false),
  m_packetsSent(0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind();
  m_socket->Connect(m_peer);
  SendPacket();
}

void
MyApp::StopApplication(void)
{
  m_running = false;

  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void
MyApp::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  Time currTime = Simulator::Now();
  if (currTime.GetSeconds() < duration) {
    ScheduleTx();
  }
}

void
MyApp::ScheduleTx(void)
{
  if (m_running)
  {
    Time tNext(Seconds(m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate())));
    m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
  }
}

int main(int argc, char** argv)
{
  uint32_t nWsnNodes = 4;
  uint32_t nFlows = 4;
  uint packetsPerSec = 300;
  uint maxRange = 100;

  Packet::EnablePrinting();

  CommandLine cmd(__FILE__);
  cmd.AddValue("nNodes", "Number of nodes", nWsnNodes);
  cmd.AddValue("nFlows", "Number of flows", nFlows);
  cmd.AddValue("packetsPerSec", "Packets per second", packetsPerSec);
  cmd.AddValue("maxRange", "Max coverage range", maxRange);
  cmd.Parse(argc, argv);

  NodeContainer wsnNodes;
  wsnNodes.Create(nWsnNodes);

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
  lrWpanHelper.SetChannel(channel);
  NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(wsnNodes);
  lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

  InternetStackHelper internetv6;
  internetv6.Install(wsnNodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install(lrwpanDevices);

  Ipv6AddressHelper ipv6;
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

  uint16_t sinkPort = 8080;
  Address sinkAddress(Inet6SocketAddress(wsnDeviceInterfaces.GetAddress(0, 1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(wsnNodes.Get(0));
  sinkApps.Start(Seconds(0));
  sinkApps.Stop(Seconds(duration));
  for (uint i = 1; i < nFlows; i++) {
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(wsnNodes.Get(i), TcpSocketFactory::GetTypeId());
    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(ns3TcpSocket, sinkAddress, 1e6 / (packetsPerSec * 8), DataRate("1Mbps"));
    wsnNodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(1));
    app->SetStopTime(Seconds(duration));
  }

  // UdpEchoServerHelper echoServer(9);
  // ApplicationContainer serverApps = echoServer.Install(wsnNodes.Get(0));
  // serverApps.Start(Seconds(0));
  // serverApps.Stop(Seconds(duration));
  // for (uint i = 1; i < nFlows; i++) {
  //   UdpEchoClientHelper echoClient(wsnDeviceInterfaces.GetAddress(0, 1), 9);
  //   echoClient.SetAttribute("MaxPackets", UintegerValue(5000));
  //   echoClient.SetAttribute("Interval", TimeValue(Seconds(1 / (double)packetsPerSec)));
  //   echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  //   ApplicationContainer clientApps = echoClient.Install(wsnNodes.Get(i));
  //   clientApps.Start(Seconds(1));
  //   clientApps.Stop(Seconds(duration));
  // }

  Simulator::Stop(Seconds(duration));

  // Flow monitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> monitor;
  monitor = flowHelper.InstallAll();

  Simulator::Run();

  // metric calculation
  double throughput = 0;
  double endToEndDelay = 0;
  int nPacketsSent = 0;
  int nPacketsReceived = 0;
  int nPacketsDropped = 0;
  int flowCount = 0;
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); iter++) {
    flowCount++;
    if (flowCount % 2) {
      throughput += iter->second.rxBytes / (1000 * (iter->second.timeLastRxPacket.GetSeconds()
        - iter->second.timeFirstTxPacket.GetSeconds()));
      endToEndDelay += iter->second.delaySum.GetSeconds();
      nPacketsSent += iter->second.txPackets;
      nPacketsReceived += iter->second.rxPackets;
      nPacketsDropped += iter->second.lostPackets;

      NS_LOG_UNCOND("Flow ID:" << iter->first);
      NS_LOG_UNCOND("Sent packets = " << iter->second.txPackets);
      NS_LOG_UNCOND("Received packets = " << iter->second.rxPackets);
      NS_LOG_UNCOND("Dropped packets = " << iter->second.lostPackets);
      NS_LOG_UNCOND("Delay sum = " << iter->second.delaySum.GetSeconds());
      NS_LOG_UNCOND("Throughput = " << iter->second.rxBytes /
        (1000 * (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds())) << " kbps\n");
    }
  }

  Simulator::Destroy();
  return 0;
}

