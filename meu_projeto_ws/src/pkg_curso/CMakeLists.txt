cmake_minimum_required(VERSION 3.8)
project(cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(example_interfaces REQUIRED)

add_executable(publisher_cpp src/publisher_cpp.cpp)
ament_target_dependencies(publisher_cpp rclcpp std_msgs)

add_executable(subscriber_cpp src/subscriber_cpp.cpp)
ament_target_dependencies(subscriber_cpp rclcpp std_msgs)

add_executable(add_two_ints_server_cpp src/add_two_ints_server.cpp)
ament_target_dependencies(add_two_ints_server_cpp rclcpp example_interfaces)

add_executable(add_two_ints_client_no_oop src/add_two_ints_client_no_oop.cpp)
ament_target_dependencies(add_two_ints_client_no_oop rclcpp example_interfaces)

add_executable(add_two_ints_client_cpp src/add_two_ints_client.cpp)
ament_target_dependencies(add_two_ints_client_cpp rclcpp example_interfaces)

install(TARGETS
  publisher_cpp
  subscriber_cpp
  add_two_ints_server_cpp
  add_two_ints_client_no_oop
  add_two_ints_client_cpp
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
