import socket
import struct

# ---------- Settings ----------
LISTEN_IP = ''               # Listen on all interfaces (for Pi packets)
LISTEN_PORT = 5005           # Port Pi is sending LiDAR packets to

FORWARD_IP = '172.22.11.169' # WSL IP from ip addr show eth0
FORWARD_PORT = 5005           # Same port WSL node binds to
# ------------------------------

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LISTEN_IP, LISTEN_PORT))  # Receive packets from Pi

print(f"Listening for LiDAR data on {LISTEN_IP}:{LISTEN_PORT}...")

while True:
    data, addr = sock.recvfrom(65535)

    # --- DEBUG: Print packet info ---
    print(f"\nReceived {len(data)} bytes from {addr}")
    num_points = len(data) // 8
    print(f"Points in this scan: {num_points}")

    for i in range(0, len(data), 8):
        angle, distance = struct.unpack('ff', data[i:i+8])
        distance_m = distance / 1000.0  # mm → meters
        print(f"Angle: {angle:.2f} rad, Distance: {distance_m:.3f} m")
    # ---------------------------------

    # Forward to WSL
    sock.sendto(data, (FORWARD_IP, FORWARD_PORT))