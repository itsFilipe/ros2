cmake_minimum_required(VERSION 3.8)
project(battery_led)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(meu_projeto_interfaces REQUIRED)

# Exemplo de criação do seu nó executável
add_executable(led_node src/led.cpp)
ament_target_dependencies(led_node rclcpp meu_projeto_interfaces)

add_executable(battery_node src/battery.cpp)
ament_target_dependencies(battery_node rclcpp meu_projeto_interfaces)

# Certifique-se de instalar o executável para o ros2 run encontrar
install(TARGETS
  led_node
  battery_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
