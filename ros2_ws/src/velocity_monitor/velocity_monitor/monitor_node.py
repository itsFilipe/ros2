import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist

class MonitorNode(Node):
    def __init__(self):
        super().__init__('monitor_node')
        
        # Se inscreve no tópico '/turtle1/cmd_vel'
        # O tipo da mensagem deve ser o mesmo usado pelo publicador C++ (Twist)
        self.subscription = self.create_subscription(
            Twist,
            '/turtle1/cmd_vel',
            self.listener_callback,
            10)
        self.subscription  # impede que a assinatura seja coletada pelo garbage collector
        
        self.get_logger().info('Nó Monitor iniciado. Aguardando dados do WalkerNode...')

    def listener_callback(self, msg):
        # Esta função é chamada toda vez que uma nova mensagem chega
        linear_v = msg.linear.x
        angular_v = msg.angular.z
        
        self.get_logger().info(
            f'LOG: Velocidade Linear: {linear_v:.2f} | Velocidade Angular: {angular_v:.2f}'
        )

def main(args=None):
    rclpy.init(args=args)
    
    monitor_node = MonitorNode()
    
    try:
        rclpy.spin(monitor_node)
    except KeyboardInterrupt:
        pass
    finally:
        monitor_node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
