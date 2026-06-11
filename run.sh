# gcc ./main.c -o main
#
if [ -z "$1" ]; then
  echo "Error: Please provide a port number."
  echo "Usage: $0 <port>"
  exit 1
fi

echo "Running with port $1"
fuser -k "$1"/udp
sudo iptables -A INPUT -p udp --dport "$1" -j ACCEPT
# kitty sh -c 'make clean && make && ./app '$1'; read'
# make clean && make && ./app "$1"; echo "exit code: $?"; read 

# DEBUG
# terminal 1 - run your app normally
make clean && make && strace -e trace=signal ./app "$1"; echo "exit code: $?"; read

# terminal 2 - watch for the kill in real time
#watch -n 0.1 'journalctl -n 20 --no-pager | tail -5'