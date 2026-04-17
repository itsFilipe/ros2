# cpp_pkg — Beginner's Guide

This package contains a publisher and a subscriber node written in C++.
It is meant to be your starting point for learning ROS2.

---

## Table of Contents

1. [What is ROS2?](#1-what-is-ros2)
2. [Project file structure](#2-project-file-structure)
3. [The boilerplate you will write in EVERY C++ ROS2 node](#3-the-boilerplate-you-will-write-in-every-c-ros2-node)
4. [publisher_cpp.cpp — explained line by line](#4-publisher_cppcpp--explained-line-by-line)
5. [subscriber_cpp.cpp — explained line by line](#5-subscriber_cppcpp--explained-line-by-line)
6. [CMakeLists.txt — what each line does](#6-cmakeliststxt--what-each-line-does)
7. [package.xml — what each line does](#7-packagexml--what-each-line-does)
8. [How to build and run](#8-how-to-build-and-run)
9. [Quick-reference cheat sheet](#9-quick-reference-cheat-sheet)

---

## 1. What is ROS2?

ROS2 (Robot Operating System 2) is **not** an operating system.
It is a framework — a set of libraries and tools — that makes it easier to build
robot software by solving problems every robotics project faces:

| Problem every robot program has | How ROS2 solves it |
|---|---|
| Multiple programs need to talk to each other | **Topics** — a publish/subscribe message bus |
| You want to call a function on another program | **Services** — request/response calls |
| You want to run a long task and track its progress | **Actions** |
| You need to manage many programs at once | **Launch files** |
| Every program needs a clock, a logger, parameters | **rclcpp::Node** base class gives you all of these |

The core idea is the **node**: a single executable program that communicates
with other nodes over topics, services, or actions. Your publisher and
subscriber are each a node.

---

## 2. Project file structure

```
cpp_pkg/
├── src/
│   ├── publisher_cpp.cpp   <- Node that sends messages
│   └── subscriber_cpp.cpp  <- Node that receives messages
├── CMakeLists.txt          <- Tells CMake how to compile the package
├── package.xml             <- Declares the package name and dependencies
└── GUIDE.md                <- This file
```

After `colcon build`, ROS2 also creates these folders at the workspace root
(do not edit them manually):

```
meu_projeto_ws/
├── build/   <- Intermediate compile output
├── install/ <- Final installed executables and headers
├── log/     <- Build logs
└── src/     <- Your source code lives here
```

---

## 3. The boilerplate you will write in EVERY C++ ROS2 node

These four things appear in every single ROS2 C++ node you will ever write.
Memorize them — they are the skeleton of every program.

```cpp
// 1. Include the ROS2 C++ client library
#include "rclcpp/rclcpp.hpp"

// 2. Make your class a Node by inheriting from rclcpp::Node
class MyNode : public rclcpp::Node {
public:
    // 3. Pass the node name string to the parent constructor
    MyNode() : Node("my_node_name") {
        // set up publishers, subscribers, timers here
    }
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);                        // 4a. Start ROS2
    rclcpp::spin(std::make_shared<MyNode>());        // 4b. Keep running
    rclcpp::shutdown();                              // 4c. Clean up
    return 0;
}
```

### Why each piece exists

**`rclcpp::Node`** — the base class that wires your code into the ROS2 system.
Without inheriting from it you have no access to topics, services, timers,
parameters, or the logger.

**`Node("my_node_name")`** — every node must have a unique name on the network.
This is what you see in `ros2 node list`.

**`rclcpp::init(argc, argv)`** — initialises the underlying communications layer
(DDS). Must be called once before anything else ROS2. It also reads any ROS2
command-line arguments passed to the executable.

**`rclcpp::spin(node)`** — hands control to ROS2. It blocks here and processes
incoming messages, timer callbacks, etc. Without `spin` your callbacks would
never execute.

**`rclcpp::shutdown()`** — releases all ROS2 resources cleanly when `spin`
returns (e.g. when you press Ctrl+C).

---

## 4. publisher_cpp.cpp — explained line by line

```cpp
#include "rclcpp/rclcpp.hpp"         // ROS2 C++ core (always needed)
#include "std_msgs/msg/string.hpp"   // The String message type
#include "std_msgs/msg/float32.hpp"  // The Float32 message type
#include <chrono>                    // C++ standard library: time durations
#include <string>                    // C++ standard library: std::string
#include <memory>                    // C++ standard library: std::make_shared
```

> Everything from `<chrono>`, `<string>`, `<memory>` is **standard C++**, not ROS2.

```cpp
using namespace std::chrono_literals;  // lets you write 1s, 500ms instead of
                                        // std::chrono::seconds(1)
```

```cpp
auto topic1 = "float_topic";   // the topic name, plain C++ string
auto topic2 = "string_topic";
```

Topic names are just strings. Any subscriber that uses the same name and the
same message type will receive your messages.

```cpp
class MinimalPublisher : public rclcpp::Node {
```

Your node IS a Node. Inheriting gives you `this->create_publisher(...)`,
`this->create_wall_timer(...)`, `this->get_logger()`, etc.

```cpp
    MinimalPublisher() : Node("minimal_publisher") {
        publisher1_ = this->create_publisher<std_msgs::msg::Float32>(topic1, 10);
        publisher2_ = this->create_publisher<std_msgs::msg::String>(topic2, 10);
```

**`create_publisher<MessageType>(topic_name, queue_size)`**
- `MessageType` — what kind of message you will send
- `topic_name` — the channel name
- `10` — the queue size (how many messages to buffer if the subscriber is slow)

```cpp
        timer1_ = this->create_wall_timer(
            1s, std::bind(&MinimalPublisher::timer_callback1, this));
```

**`create_wall_timer(interval, callback)`**
Fires the callback every `interval`. Uses real wall-clock time (as opposed to
simulated time). `std::bind` turns a member function + `this` into a plain
callable that ROS2 can store and invoke.

```cpp
    void timer_callback1() {
        auto message = std_msgs::msg::Float32();
        message.data = 1.0;
        publisher1_->publish(message);
    }
```

This is pure C++ plus the ROS2 message struct.
`msg.data` is the field name defined by the `std_msgs/Float32` message type.
Every message type has its own fields — check the docs or run
`ros2 interface show std_msgs/msg/Float32`.

---

## 5. subscriber_cpp.cpp — explained line by line

```cpp
using std::placeholders::_1;  // shorthand used with std::bind for callbacks
                               // that receive an argument (the incoming message)
```

```cpp
        subscription1_ = this->create_subscription<std_msgs::msg::Float32>(
            topic1, 10, std::bind(&MinimalSubscriber::topic_callback1, this, _1));
```

**`create_subscription<MessageType>(topic_name, queue_size, callback)`**
- Same topic name and message type as the publisher — this is how they connect.
- `_1` is a placeholder meaning "pass the first argument (the message) through
  to the callback".

```cpp
    void topic_callback1(const std_msgs::msg::Float32::SharedPtr msg) const {
        RCLCPP_INFO(this->get_logger(), "I heard: '%f'", msg->data);
    }
```

**`RCLCPP_INFO(logger, format, ...)`** — ROS2 logger. Works like `printf` but
routes through the ROS2 logging system so messages appear with timestamps and
severity levels. Other levels: `RCLCPP_WARN`, `RCLCPP_ERROR`, `RCLCPP_DEBUG`.

`msg` is a `SharedPtr` (a `std::shared_ptr`) — standard C++ smart pointer,
not ROS2-specific. You access fields with `msg->data` instead of `msg.data`
because it is a pointer.

---

## 6. CMakeLists.txt — what each line does

```cmake
cmake_minimum_required(VERSION 3.8)   # minimum CMake version, standard C++
project(cpp_pkg)                      # must match the name in package.xml
```

```cmake
find_package(ament_cmake REQUIRED)    # ROS2 build system (always needed)
find_package(rclcpp REQUIRED)         # ROS2 C++ client library
find_package(std_msgs REQUIRED)       # standard message types
```

These tell CMake where to find the ROS2 headers and libraries.
They only work because you sourced the ROS2 environment first (`source /opt/ros/humble/setup.bash`).

```cmake
add_executable(publisher_cpp src/publisher_cpp.cpp)
ament_target_dependencies(publisher_cpp rclcpp std_msgs)
```

- `add_executable` — standard CMake: compile this .cpp into a binary.
- `ament_target_dependencies` — ROS2 version of `target_link_libraries`.
  It sets up **both** the include paths (so headers are found) and the linker
  flags. Always use this instead of plain `target_link_libraries` for ROS2
  packages.

```cmake
install(TARGETS publisher_cpp subscriber_cpp
  DESTINATION lib/${PROJECT_NAME}
)
```

Copies the compiled binaries into the `install/` folder.
Without this, `ros2 run cpp_pkg publisher_cpp` will not find the executable.

```cmake
ament_package()   # always the last line — finalises the ROS2 package
```

---

## 7. package.xml — what each line does

```xml
<name>cpp_pkg</name>          <!-- must match project() in CMakeLists.txt -->
<version>0.0.0</version>
<maintainer email="...">filipe</maintainer>
<license>TODO</license>
```

```xml
<buildtool_depend>ament_cmake</buildtool_depend>
```

The build tool itself. Always present in C++ ROS2 packages.

```xml
<depend>rclcpp</depend>
<depend>std_msgs</depend>
```

`<depend>` means this package needs these both to **build** and to **run**.
(The old code used `<exec_depend>` which is runtime-only and would cause build
failures.)

```xml
<export>
  <build_type>ament_cmake</build_type>
</export>
```

Tells `colcon` this is a CMake-based package (as opposed to a Python package
which uses `ament_python`).

---

## 8. How to build and run

Every terminal session you open, source ROS2 first:

```bash
source /opt/ros/humble/setup.bash
```

Build:

```bash
cd ~/meu_projeto_ws
colcon build --packages-select cpp_pkg
source install/setup.bash   # make the new executables visible
```

Run in two separate terminals (both sourced):

```bash
# Terminal 1
ros2 run cpp_pkg publisher_cpp

# Terminal 2
ros2 run cpp_pkg subscriber_cpp
```

Useful commands while they are running:

```bash
ros2 node list                         # see all active nodes
ros2 topic list                        # see all active topics
ros2 topic echo /float_topic           # print messages on a topic live
ros2 topic info /float_topic           # who is publishing/subscribing
ros2 interface show std_msgs/msg/Float32  # see the fields of a message type
```

---

## 9. Quick-reference cheat sheet

| What you want to do | ROS2 C++ call |
|---|---|
| Create a node | `class MyNode : public rclcpp::Node` |
| Name the node | `: Node("name")` in the constructor |
| Publish messages | `this->create_publisher<MsgType>("topic", queue)` |
| Subscribe to messages | `this->create_subscription<MsgType>("topic", queue, callback)` |
| Run something on a timer | `this->create_wall_timer(duration, callback)` |
| Log info | `RCLCPP_INFO(this->get_logger(), "text %d", value)` |
| Log a warning | `RCLCPP_WARN(this->get_logger(), "text")` |
| Log an error | `RCLCPP_ERROR(this->get_logger(), "text")` |
| Start ROS2 | `rclcpp::init(argc, argv)` |
| Keep the node alive | `rclcpp::spin(node)` |
| Stop ROS2 cleanly | `rclcpp::shutdown()` |

### Message types you will use often

| Type | Include | Field |
|---|---|---|
| `std_msgs::msg::String` | `std_msgs/msg/string.hpp` | `.data` (std::string) |
| `std_msgs::msg::Float32` | `std_msgs/msg/float32.hpp` | `.data` (float) |
| `std_msgs::msg::Float64` | `std_msgs/msg/float64.hpp` | `.data` (double) |
| `std_msgs::msg::Int32` | `std_msgs/msg/int32.hpp` | `.data` (int32) |
| `std_msgs::msg::Bool` | `std_msgs/msg/bool.hpp` | `.data` (bool) |
| `geometry_msgs::msg::Twist` | `geometry_msgs/msg/twist.hpp` | `.linear.x`, `.angular.z` |

`geometry_msgs/Twist` is the standard way to command a robot's velocity — you
will use it constantly in mobile robotics.
