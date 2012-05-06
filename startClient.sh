killall Timer
killall TCPD
killall ftps
killall ftpc
killall troll
#troll -b 15000 -s0 -x0&
troll -b 15000 -x25 -se 10 -g25 -m25 -r&
make clean
make
Timer&
TCPD&
ftpc $1 $2 $3
