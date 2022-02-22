#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RtoCompDemo");

static bool firstRtt = true;
static bool firstRto = true;
static Ptr<OutputStreamWrapper> rttStream;
static Ptr<OutputStreamWrapper> rtoStream;
static Ptr<OutputStreamWrapper> throughputStream;
static Ptr<OutputStreamWrapper> packetDropStream;
uint32_t total_phy_packet_sent = 0;
uint32_t total_phy_packet_received = 0;
uint32_t total_phy_packet_dropped = 0;

uint32_t total_tcp_packet_sent = 0;
uint32_t total_tcp_packet_received = 0;
uint64_t total_received_bytes = 0;

static void
RttTracer (Time oldval, Time newval)
{
  if (firstRtt)
    {
      *rttStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRtt = false;
    }
  *rttStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

static void
RtoTracer (Time oldval, Time newval)
{
  if (firstRto)
    {
      *rtoStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRto = false;
    }
  *rtoStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

static void
TxEndTracer (Ptr<const Packet> p)
{
  total_phy_packet_sent += 1;
}

static void
RxEndTracer (Ptr<const Packet> p)
{
  total_phy_packet_received += 1;
}

static void
RxDropTracer (Ptr<const Packet> p)
{
  total_phy_packet_dropped += 1;
  *packetDropStream->GetStream() << Simulator::Now().GetSeconds() << " " << 1 << std::endl;
}

static void
TxTracer(const Ptr< const Packet > packet, const TcpHeader &header, const Ptr< const TcpSocketBase > socket) {
  total_tcp_packet_sent += 1;
}

static void
RxTracer(const Ptr< const Packet > packet, const TcpHeader &header, const Ptr< const TcpSocketBase > socket) {
  total_tcp_packet_received += 1;
  total_received_bytes += packet->GetSize();
}

static void
TraceRtt (std::string rtt_tr_file_name)
{
  AsciiTraceHelper ascii;
  rttStream = ascii.CreateFileStream (rtt_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback (&RttTracer));
}

static void
TraceRto (std::string rto_tr_file_name)
{
  AsciiTraceHelper ascii;
  rtoStream = ascii.CreateFileStream (rto_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTO", MakeCallback (&RtoTracer));
}

static void
TraceRxDrop (std::string rxdrop_tr_file_name, NetDeviceContainer s_devices)
{
  AsciiTraceHelper ascii;
  packetDropStream = ascii.CreateFileStream (rxdrop_tr_file_name.c_str ());
  s_devices.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxEndTracer));
  s_devices.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxEndTracer));
  s_devices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&RxDropTracer));
}

static void
TraceTxRx (int num_flows)
{
  AsciiTraceHelper ascii;
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/Tx", MakeCallback (&TxTracer));
  Config::ConnectWithoutContext ("/NodeList/" + std::to_string(num_flows) + "/$ns3::TcpL4Protocol/SocketList/0/Rx", MakeCallback (&RxTracer));
}

static void
SendBurst (Ptr<NetDevice> sourceDevice, Address& destination) {
  for (uint16_t i = 0; i < 100; i++) {
      Ptr<Packet> pkt = Create<Packet> (1000); // 1000 dummy bytes of data
      sourceDevice->Send (pkt, destination, 0);
  }
}

int main (int argc, char *argv[])
{
  std::string transport_prot = "TcpWestwood";
  double error_p = 0.0;
  std::string bottleneck_bandwidth = "10Mbps";
  std::string bottleneck_delay = "35ms";
  std::string edge_bandwidth = "2Mbps";
  std::string edge_delay = "175ms";
  
  bool tracing = true;
  std::string prefix_file_name = "RtoCompDemo";
  uint64_t data_mbytes = 0;
  uint32_t mtu_bytes = 1000;//400
  uint16_t num_flows = 1;
  double duration = 5;
  uint32_t run = 0;
  bool flow_monitor = false;
  bool pcap = false;
  bool sack = true;
  std::string queue_disc_type = "ns3::PfifoFastQueueDisc";
  std::string recovery = "ns3::TcpClassicRecovery";
  bool modified_rtt_calc = false;


  CommandLine cmd (__FILE__);
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpNewReno, TcpLinuxReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, "
		"TcpLp, TcpDctcp, TcpCubic, TcpBbr", transport_prot);
  cmd.AddValue ("error_p", "Packet error rate", error_p);
  cmd.AddValue ("bottleneck_bandwidth", "Bottleneck bandwidth", bottleneck_bandwidth);
  cmd.AddValue ("bottleneck_delay", "Bottleneck delay", bottleneck_delay);
  cmd.AddValue ("edge_bandwidth", "Edge link bandwidth", edge_bandwidth);
  cmd.AddValue ("edge_delay", "Edge link delay", edge_delay);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("prefix_name", "Prefix of output trace file", prefix_file_name);
  cmd.AddValue ("data", "Number of Megabytes of data to transmit", data_mbytes);
  cmd.AddValue ("mtu", "Size of IP packets to send in bytes", mtu_bytes);
  cmd.AddValue ("num_flows", "Number of flows", num_flows);
  cmd.AddValue ("duration", "Time to allow flows to run in seconds", duration);
  cmd.AddValue ("run", "Run index (for setting repeatable seeds)", run);
  cmd.AddValue ("flow_monitor", "Enable flow monitor", flow_monitor);
  cmd.AddValue ("pcap_tracing", "Enable or disable PCAP tracing", pcap);
  cmd.AddValue ("queue_disc_type", "Queue disc type for gateway (e.g. ns3::CoDelQueueDisc)", queue_disc_type);
  cmd.AddValue ("sack", "Enable or disable SACK option", sack);
  cmd.AddValue ("recovery", "Recovery algorithm type to use (e.g., ns3::TcpPrrRecovery", recovery);
  cmd.AddValue ("modified_rtt_calc", "Modification in RTT calculation", modified_rtt_calc);
  cmd.Parse (argc, argv);

  transport_prot = std::string ("ns3::") + transport_prot;

  SeedManager::SetSeed (1);
  SeedManager::SetRun (run);

  // User may find it convenient to enable logging
  //LogComponentEnable("RtoCompDemo", LOG_LEVEL_ALL);
  //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("PfifoFastQueueDisc", LOG_LEVEL_ALL);

  // Calculate the ADU size
  Header* temp_header = new Ipv4Header ();
  uint32_t ip_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("IP Header size is: " << ip_header);
  delete temp_header;
  temp_header = new TcpHeader ();
  uint32_t tcp_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("TCP Header size is: " << tcp_header);
  delete temp_header;
  uint32_t tcp_adu_size = mtu_bytes - 20 - (ip_header + tcp_header);
  NS_LOG_LOGIC ("TCP ADU size is: " << tcp_adu_size);

  // Set the simulation start and stop time
  double start_time = 0.1;
  double stop_time = start_time + duration;

  // 2 MB of TCP buffer
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));

  // Use modified version of RTT calculation if enabled
  if(modified_rtt_calc){
    Config::SetDefault("ns3::RttMeanDeviation::Modified_RTT_Calc", BooleanValue(modified_rtt_calc));
  }

  Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType",
                      TypeIdValue (TypeId::LookupByName (recovery)));
  // Select TCP variant
  if (transport_prot.compare ("ns3::TcpWestwoodPlus") == 0)
    { 
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid), "TypeId " << transport_prot << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (transport_prot)));
    }

  // Create routers, sources, and sinks
  NodeContainer sources;
  sources.Create (num_flows);
  NodeContainer sinks;
  sinks.Create (num_flows);
  NodeContainer routers;
  routers.Create (2);

  // Configure the error model
  // Here we use RateErrorModel with packet error rate
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  RateErrorModel error_model;
  error_model.SetRandomVariable (uv);
  error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model.SetRate (error_p);

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bottleneck_bandwidth));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue (bottleneck_delay));
  bottleneckLink.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (&error_model));

  InternetStackHelper stack;
  stack.InstallAll ();

  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");

  TrafficControlHelper tchCoDel;
  tchCoDel.SetRootQueueDisc ("ns3::CoDelQueueDisc");

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  // Configure the sources and sinks net devices
  // and the channels between the sources/sinks and the gateways
  PointToPointHelper sourceLink;
  sourceLink.SetDeviceAttribute ("DataRate", StringValue (edge_bandwidth));
  sourceLink.SetChannelAttribute ("Delay", StringValue (edge_delay));

  PointToPointHelper sinkLink;
  sinkLink.SetDeviceAttribute ("DataRate", StringValue (edge_bandwidth));
  sinkLink.SetChannelAttribute ("Delay", StringValue (edge_delay));
  //sinkLink.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (&error_model));

  Ipv4InterfaceContainer sink_interfaces;

  DataRate edge_b (edge_bandwidth);
  DataRate bottle_b (bottleneck_bandwidth);
  Time edge_d (edge_delay);
  Time bottle_d (bottleneck_delay);

  uint32_t size = static_cast<uint32_t>((std::min (edge_b, bottle_b).GetBitRate () / 8) *
    ((edge_d + bottle_d) * 2).GetSeconds ());

  Config::SetDefault ("ns3::PfifoFastQueueDisc::MaxSize",
                      QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, size / mtu_bytes)));
  Config::SetDefault ("ns3::CoDelQueueDisc::MaxSize",
                      QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, size)));

  NetDeviceContainer routerDevices = bottleneckLink.Install (routers.Get (0), routers.Get (1));
  address.Assign(routerDevices);
  NetDeviceContainer sourceDevices[num_flows];
  NetDeviceContainer sinkDevices[num_flows];

  for (uint32_t i = 0; i < num_flows; i++)
    {
      sourceDevices[i] = sourceLink.Install (sources.Get (i), routers.Get (0));
      tchPfifo.Install (sourceDevices[i]);
      address.NewNetwork ();
      Ipv4InterfaceContainer interfaces = address.Assign (sourceDevices[i]);

      sinkDevices[i] = sinkLink.Install (routers.Get (1), sinks.Get (i));
      if (queue_disc_type.compare ("ns3::PfifoFastQueueDisc") == 0)
        {
          tchPfifo.Install (sinkDevices[i]);
        }
      else if (queue_disc_type.compare ("ns3::CoDelQueueDisc") == 0)
        {
          tchCoDel.Install (sinkDevices[i]);
        }
      else
        {
          NS_FATAL_ERROR ("Queue not recognized. Allowed values are ns3::CoDelQueueDisc or ns3::PfifoFastQueueDisc");
        }
      address.NewNetwork ();
      interfaces = address.Assign (sinkDevices[i]);
      sink_interfaces.Add (interfaces.Get (1));
    }

  NS_LOG_INFO ("Initialize Global Routing.\n");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp;

  for (uint16_t i = 0; i < sources.GetN (); i++)
    {
      Address addr (InetSocketAddress (sink_interfaces.GetAddress (i, 0), port));
      AddressValue remoteAddress (InetSocketAddress (sink_interfaces.GetAddress (i, 0), port));
      Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcp_adu_size));
      BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());
      ftp.SetAttribute ("Remote", remoteAddress);
      ftp.SetAttribute ("SendSize", UintegerValue (tcp_adu_size));
      ftp.SetAttribute ("MaxBytes", UintegerValue (data_mbytes * 1000000));

      ApplicationContainer sourceApp = ftp.Install (sources.Get (i));
      sourceApp.Start (Seconds (start_time * i));
      sourceApp.Stop (Seconds (stop_time - 3));

      sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
      sinkApp = sinkHelper.Install (sinks.Get (i));
      sinkApp.Start (Seconds (start_time * i));
      sinkApp.Stop (Seconds (stop_time));
      Simulator::Schedule(Seconds(duration/2), &SendBurst, sourceDevices[i].Get(0), addr);
    }

  // Set up tracing if enabled
  if (tracing)
    {
      AsciiTraceHelper ascii;
      //bottleneckLink.EnableAsciiAll(ascii.CreateFileStream("throughput.tr"));
      Simulator::Schedule (Seconds (0.00001), &TraceRtt, prefix_file_name + "-rtt.data");
      Simulator::Schedule (Seconds (0.00001), &TraceRto, prefix_file_name + "-rto.data");
      Simulator::Schedule (Seconds (0.00001), &TraceRxDrop, prefix_file_name + "-rxdrop.data", routerDevices);
      Simulator::Schedule (Seconds (0.00001), &TraceTxRx, num_flows);
    }

  if (pcap)
    {
      bottleneckLink.EnablePcapAll (prefix_file_name, true);
      sourceLink.EnablePcapAll (prefix_file_name, true);
    }

  // Flow monitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> monitor;
  if (flow_monitor)
  {
    monitor = flowHelper.InstallAll ();
  }

  Simulator::Stop (Seconds (stop_time));
  Simulator::Run ();

  //metric calculation
  if (flow_monitor) {
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    uint16_t count = 0;
    for(auto iter = stats.begin(); iter != stats.end(); iter++) {
      if(count == 1) break;
      NS_LOG_UNCOND("Flow ID:" <<iter->first);
      NS_LOG_UNCOND("Sent Packets = " << iter->second.txPackets);
      NS_LOG_UNCOND("Received Packets = " << iter->second.rxPackets);
      NS_LOG_UNCOND("Lost Packets = " << iter->second.txPackets - iter->second.rxPackets);
      NS_LOG_UNCOND("Packet loss ratio = " << (iter->second.txPackets-iter->second.rxPackets)*100.0/iter->second.txPackets << "%");
      NS_LOG_UNCOND("Throughput = " << iter->second.rxBytes * 8.0/
        (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) << " bits/s\n");
      count++;
    }
  }

  Simulator::Destroy ();

  //metric calculation
  NS_LOG_UNCOND("PHY:\nTotal packets sent: " << total_phy_packet_sent);
  NS_LOG_UNCOND("Total packets received: " << total_phy_packet_received);
  NS_LOG_UNCOND("Total packets dropped: " << total_phy_packet_dropped);
  NS_LOG_UNCOND("Packet Delivery Ratio: " << total_phy_packet_received / (float)total_phy_packet_sent);
  NS_LOG_UNCOND("Packet Drop Ratio: " << total_phy_packet_dropped / (float)total_phy_packet_sent);

  NS_LOG_UNCOND("\n(Flow 0)\nTotal packets sent: " << total_tcp_packet_sent);
  NS_LOG_UNCOND("Total packets received: " << total_tcp_packet_received);
  uint32_t total_tcp_packet_dropped = total_tcp_packet_sent - total_tcp_packet_received;
  NS_LOG_UNCOND("Total packets dropped: " << total_tcp_packet_dropped);
  NS_LOG_UNCOND("Packet Drop Ratio: " << total_tcp_packet_dropped / (float)total_tcp_packet_sent);
  NS_LOG_UNCOND("Throughput: " << total_received_bytes * 8 / (float)stop_time << " bits/s");
  return 0;
}
