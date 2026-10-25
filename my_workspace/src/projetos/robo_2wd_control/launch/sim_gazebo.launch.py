"""
sim_gazebo.launch.py

Abre o Gazebo (Harmonic / ROS 2 Jazzy) com um mundo vazio,
carrega o URDF do robo_2wd, spawna o carrinho 3D no mundo
e cria a ponte (ros_gz_bridge) para você dirigir com o wasd_teleop_node!
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    pkg_share = get_package_share_directory('robo_2wd_control')
    urdf_path = os.path.join(pkg_share, 'urdf', 'robo_2wd.urdf')

    with open(urdf_path, 'r') as infp:
        robot_desc = infp.read()

    # 1. Publica a árvore de peças (TF) e o modelo 3D do URDF no ROS 2
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': robot_desc,
            'use_sim_time': True,
        }]
    )

    # 2. Abre o simulador Gazebo com um mundo vazio (empty.sdf)
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('ros_gz_sim'),
                'launch',
                'gz_sim.launch.py'
            )
        ),
        launch_arguments={'gz_args': '-r empty.sdf'}.items(),
    )

    # 3. Coloca ("Spawna") o nosso carrinho dentro do mundo do Gazebo
    spawn_robot = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=[
            '-name', 'robo_2wd',
            '-topic', 'robot_description',
            '-x', '0.0',
            '-y', '0.0',
            '-z', '0.05',
        ],
        output='screen',
    )

    # 4. Ponte ROS 2 <-> Gazebo: conecta o /cmd_vel do seu teclado WASD ao Gazebo!
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '/cmd_vel@geometry_msgs/msg/Twist@gz.msgs.Twist',
            '/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            '/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V',
            '/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model',
            '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock',
        ],
        output='screen',
    )

    return LaunchDescription([
        robot_state_publisher,
        gz_sim,
        spawn_robot,
        bridge,
    ])
