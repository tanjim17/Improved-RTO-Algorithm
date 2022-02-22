#!/bin/bash
# for((i=3;i<=30;i+=3))
# do
#     ./waf --run "scratch/rto-comp-demo.cc --modified_rtt_calc=false --tracing=true --duration=20 --edge_bandwidth=${i}Mbps"
#     gnuplot -e "filename='non_edge_band_$i'; ran='$j'" plot.plt
# done

# for((i=3;i<=30;i+=3))
# do
#     ./waf --run "scratch/rto-comp-demo.cc --modified_rtt_calc=false --tracing=true --duration=20 --bottleneck_bandwidth=${i}ms"
#     gnuplot -e "filename='non_bottle_band_$i'; ran='$j'" plot.plt
# done

# for((i=30;i<=300;i+=30))
# do
#     ./waf --run "scratch/rto-comp-demo.cc --modified_rtt_calc=false --tracing=true --duration=20 --edge_delay=${i}ms"
#     gnuplot -e "filename='non_edge_delay_$i'; ran='$j'" plot.plt
# done

# for((i=30;i<=300;i+=30))
# do
#     ./waf --run "scratch/rto-comp-demo.cc --modified_rtt_calc=false --tracing=true --duration=20 --bottleneck_delay=${i}ms"
#     gnuplot -e "filename='non_bottle_delay_$i'; ran='$j'" plot.plt
# done --nNodes=$i --nFlows=$i

for((j=13;j<=13;j+=13))
do
    for((i=1;i<=8;i+=1))
    do  
        echo "cov = $i"
        ./waf --run "scratch/wireless-demo-tcp --nNodes=2 --nFlows=2 --packetsPerSec=300 --maxRange=$i --delta=0.4"
    done
    gnuplot -e "filename='throughput'; metric='Throughput'; unit='KBPS'; ran='$j'" plot.plt
    gnuplot -e "filename='delay'; metric='End-to-end delay';  unit='second'; ran='$j'" plot.plt
    gnuplot -e "filename='delivery'; metric='Packet Delivery Ratio'; unit='%'; ran='$j'" plot.plt
    gnuplot -e "filename='drop'; metric='Packet Drop Ratio'; unit='%'; ran='$j'" plot.plt
    rm *.data
done