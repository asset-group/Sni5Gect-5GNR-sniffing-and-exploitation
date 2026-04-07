## uhd_rx_cfile
## N78 20MHz
# ./scripts/uhd_rx_cfile -r 23.04e6 -f 3427.5e6 -g 31 -a type=x300,master_clock_rate=184.32e6,sampling_rate=23.04e6 --clock-source=gpsdo records/output.fc32
# ./scripts/uhd_rx_cfile -r 23.04e6 -f 3427.5e6 -g 40 -a type=b200 --clock-source=external /root/records/output.fc32

## N78 20MHz
# ./build/shadower/tools/recorder -f 3427.5 -g 37 -s 23.04 -n 20000 -t uhd -d type=x300,master_clock_rate=184.32e6,clock=gpsdo -O records/ -o output
# ./build/shadower/tools/recorder -f 3427.5 -g 40 -s 23.04 -n 20000 -t uhd -d type=b200,clock=external -O records/ -o output

# Starhub N1
# ./build/shadower/tools/recorder -f 2157.5 -g 37 -F 1967.5 -G 24 -s 46.08 -n 20000 -t uhd -d type=x300,master_clock_rate=184.32e6,address=192.168.40.3,clock=gpsdo,sync=gpsdo -O records/ -o output
# ./scripts/uhd_rx_cfile -r 46.08e6 -f 2157.5e6 -g 37 -F 1967.5e6 -G 24 -c 0,1 -a type=x300,master_clock_rate=184.32e6,address=192.168.40.3 --clock-source=gpsdo records/channel.fc32