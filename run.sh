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
kitty sh -c 'make clean && make && ./app '$1'; read'
