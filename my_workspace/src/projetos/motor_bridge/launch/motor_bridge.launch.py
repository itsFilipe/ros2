"""
launch/motor_bridge.launch.py

Launch file para o motor_bridge_node.

ROS2 Jazzy usa launch files Python por padrão.
Um launch file permite:
  - Iniciar múltiplos nodes de uma vez
  - Passar parâmetros sem precisar de --ros-args na linha de comando
  - Compor sistemas maiores (ex: incluir outros launch files)

USO:
  ros2 launch motor_bridge motor_bridge.launch.py
  ros2 launch motor_bridge motor_bridge.launch.py serial_port:=/dev/ttyUSB0
  ros2 launch motor_bridge motor_bridge.launch.py serial_port:=/dev/ttyACM0 baud_rate:=115200
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    # ── Argumentos declarados ────────────────────────────────────────────────
    # DeclareLaunchArgument expõe argumentos que podem ser sobrescritos
    # na linha de comando (ros2 launch ... serial_port:=/dev/ttyUSB0).
    # default_value define o valor padrão se nenhum for passado.
    serial_port_arg = DeclareLaunchArgument(
        "serial_port",
        default_value="/dev/ttyACM0",
        description="Porta serial do Arduino (ex: /dev/ttyACM0, /dev/ttyUSB0)",
    )

    baud_rate_arg = DeclareLaunchArgument(
        "baud_rate",
        default_value="9600",
        description="Baud rate da comunicação serial (deve coincidir com o firmware do Arduino)",
    )

    # ── Definição do node ────────────────────────────────────────────────────
    motor_bridge_node = Node(
        package="motor_bridge",
        executable="motor_bridge_node",
        name="motor_bridge_node",
        output="screen",   # exibe logs no terminal em vez de só em arquivo
        parameters=[
            {
                # LaunchConfiguration recupera o valor do argumento em runtime
                "serial_port": LaunchConfiguration("serial_port"),
                "baud_rate": LaunchConfiguration("baud_rate"),
            }
        ],
    )

    return LaunchDescription([
        serial_port_arg,
        baud_rate_arg,
        motor_bridge_node,
    ])
