cmake_minimum_required(VERSION 2.8.3)
project(phidget_stepper)

find_package(catkin REQUIRED COMPONENTS corobot_msgs roscpp corobot_diagnostics)

catkin_package(
   INCLUDE_DIRS include
   LIBRARIES ${PROJECT_NAME}
   CATKIN_DEPENDS roscpp corobot_diagnostics)

include_directories(include ${catkin_INCLUDE_DIRS})

add_executable(phidget_stepper src/PhidgetStepper.cpp)
target_link_libraries(phidget_stepper phidget21)
target_link_libraries(phidget_stepper ${catkin_LIBRARIES})
add_dependencies(phidget_stepper corobot_msgs_gencpp) 
