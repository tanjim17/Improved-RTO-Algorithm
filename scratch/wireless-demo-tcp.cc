#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/wifi-net-device.h"
#include "ns3/ap-wifi-mac.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessDemo");

double duration = 20;

void writeToFile(std::ofstream& stream, int parameter, double value, std::string fileName) {
    stream.open(fileName + ".data", std::ios_base::app);
    stream << parameter << " " << value << std::endl;
    stream.close();
}

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

int
main(int argc, char* argv[])
{
    uint32_t nNodes = 4;
    uint32_t nFlows = 4;
    double maxRange = 4;
    double delta = 0.4;
    uint32_t packetsPerSec = 300;
    bool flow_monitor = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of wifi STA devices on each side", nNodes);
    cmd.AddValue("nFlows", "Number of flows", nFlows);
    cmd.AddValue("maxRange", "Max coverage range", maxRange);
    cmd.AddValue("packetsPerSec", "Packets per second", packetsPerSec);
    cmd.AddValue("delta", "Value of deltaX and deltaY in mobility grid position", delta);
    cmd.AddValue("flow_monitor", "Enable flow monitor", flow_monitor);

    cmd.Parse(argc, argv);

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

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
    Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(maxRange));
    channel1.AddPropagationLoss("ns3::RangePropagationLossModel");
    channel2.AddPropagationLoss("ns3::RangePropagationLossModel");
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

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(delta),
        "DeltaY", DoubleValue(delta),
        "GridWidth", UintegerValue(5),
        "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sourceNodes);
    mobility.Install(sinkNodes);
    mobility.Install(apNodes);

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

    // duration += nFlows * 0.1;
    for (uint i = 0; i < nFlows; i++) {
        uint16_t sinkPort = 8080;
        Address sinkAddress(InetSocketAddress(sinkInterfaces.GetAddress(i), sinkPort));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
        ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNodes.Get(i));
        sinkApps.Start(Seconds(0));
        sinkApps.Stop(Seconds(duration));

        Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(sourceNodes.Get(i), TcpSocketFactory::GetTypeId());
        Ptr<MyApp> app = CreateObject<MyApp>();
        app->Setup(ns3TcpSocket, sinkAddress, 1e6 / (packetsPerSec * 8), DataRate("1Mbps"));
        sourceNodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1));
        app->SetStopTime(Seconds(duration));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(duration));

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
    }

    std::ofstream througputStream, delayStream, deliveryStream, dropStream;
    writeToFile(througputStream, maxRange, throughput / nNodes, "throughput");
    writeToFile(delayStream, maxRange, endToEndDelay / nPacketsReceived, "delay");
    writeToFile(deliveryStream, maxRange, nPacketsReceived * 100 / (double)nPacketsSent, "delivery");
    writeToFile(dropStream, maxRange, nPacketsDropped * 100 / (double)nPacketsSent, "drop");

    Simulator::Destroy();
    return 0;
}