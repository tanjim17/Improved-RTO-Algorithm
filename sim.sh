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

id="54"
for((j=1;j<=3;j+=1))
do
    for((i=1;i<=5;i+=1))
    do  
        echo "maxRange = $i"
        ./waf --run "scratch/mywpan --nNodes=5 --nFlows=5 --delta=0.$j --maxRange=$i"
    done
    gnuplot -e "filename='throughput'; metric='Throughput'; unit='KBPS'; ran='${id}_$j'" plot.plt
    gnuplot -e "filename='delay'; metric='End-to-end delay';  unit='second'; ran='${id}_$j'" plot.plt
    gnuplot -e "filename='delivery'; metric='Packet Delivery Ratio'; unit='%'; ran='${id}_$j'" plot.plt
    gnuplot -e "filename='drop'; metric='Packet Drop Ratio'; unit='%'; ran='${id}_$j'" plot.plt
    rm *.data
done