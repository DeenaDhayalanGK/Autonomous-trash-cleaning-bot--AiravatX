from rplidar import RPLidar
import socket
import struct

WINDOWS_IP = '10.229.138.62'  # your Windows IP
PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
lidar = RPLidar('/dev/ttyUSB0', baudrate=460800)

print("Streaming LiDAR to Windows...")

try:
    for scan in lidar.iter_scans():
        data = b''
        for (_, angle, distance) in scan:
            data += struct.pack('ff', angle, distance)  # angle + distance as f>
        sock.sendto(data, (WINDOWS_IP, PORT))
except KeyboardInterrupt:
    print("Stopping LiDAR stream...")
finally:
    lidar.stop()
    lidar.disconnect()