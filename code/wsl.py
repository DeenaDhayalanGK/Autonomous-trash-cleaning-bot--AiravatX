mport rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
import socket
import struct
import math

UDP_PORT = 5005  # Must match Windows forwarder

class LidarReceiver(Node):
    def _init_(self):
        super()._init_('lidar_receiver_node')
        self.publisher_ = self.create_publisher(LaserScan, '/scan', 10)

        # Setup UDP socket to listen on all interfaces
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('0.0.0.0', UDP_PORT))  # Receive from Windows forwarder
        self.sock.setblocking(False)

        self.get_logger().info(f"Listening for LiDAR packets on 0.0.0.0:{UDP_PORT}")

        # ROS 2 timer to periodically check for UDP packets
        self.timer = self.create_timer(0.05, self.timer_callback)  # 20 Hz

    def timer_callback(self):
        try:
            while True:
                data, addr = self.sock.recvfrom(65535)
                self.process_scan(data)
        except BlockingIOError:
            pass  # no more packets

    def process_scan(self, data):
        msg = LaserScan()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = "laser"
        msg.angle_min = 0.0
        msg.angle_max = 2 * math.pi
        msg.angle_increment = (2 * math.pi) / (len(data) // 8)
        msg.time_increment = 0.0
        msg.scan_time = 0.05
        msg.range_min = 0.15
        msg.range_max = 8.0

        # unpack each point
        ranges = []
        for i in range(0, len(data), 8):
            angle, distance = struct.unpack('ff', data[i:i+8])
            ranges.append(distance / 1000.0)  # convert mm → meters

        msg.ranges = ranges
        msg.intensities = [0.0] * len(ranges)

        self.publisher_.publish(msg)
        self.get_logger().info(f"Published scan with {len(ranges)} points")

def main(args=None):
    rclpy.init(args=args)
    node = LidarReceiver()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if _name_ == '_main_':
    main()