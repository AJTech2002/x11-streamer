while true; do
    echo "$(date +%T) $(cat /sys/fs/cgroup/user.slice/user-$(id -u).slice/memory.current)"
    sleep 0.2
done
