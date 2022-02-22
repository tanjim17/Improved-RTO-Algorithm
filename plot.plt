# set terminal png
# set output "abc.png" #filename
# set title "rtt/rto vs time"
# set xlabel "time (sec)"
# set ylabel "rtt/rto (sec)"
# set grid
# plot "RtoCompDemo-rtt.data" with lines title "rtt", "RtoCompDemo-rto.data" with lines title "rto"

# # set yrange [0:100]
# # set xrange [0:10]


set terminal png
set output "abc.png"
set title "ABC"
plot sin(x)*100 title "" with linespoints pointtype 2


# set terminal png
# set output filename.".png"
# set title metric." vs  of No. of Nodes"
# set xlabel "no. of nodes"
# set ylabel unit
# set grid
# set xrange [1:]
# plot filename.".data" with lines notitle, linespoints pointtype 2