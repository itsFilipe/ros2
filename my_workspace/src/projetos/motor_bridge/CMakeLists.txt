cmake_minimum_required(VERSION 3.8)
project(motor_bridge)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Dependências ---
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# --- Executável principal ---
# Compila motor_bridge_node a partir de um único arquivo fonte.
# Não há dependências de libs externas além de rclcpp e std_msgs —
# a comunicação serial usa termios.h que já faz parte da libc do Linux.
add_executable(motor_bridge_node src/motor_bridge_node.cpp)
ament_target_dependencies(motor_bridge_node rclcpp std_msgs)

# --- Instalação ---
# O executável precisa ir para lib/${PROJECT_NAME} para o
# `ros2 run motor_bridge motor_bridge_node` funcionar.
install(TARGETS
  motor_bridge_node
  DESTINATION lib/${PROJECT_NAME}
)

# Instala o launch file para que `ros2 launch motor_bridge ...` funcione.
install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

# --- Testes (padrão do workspace) ---
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
