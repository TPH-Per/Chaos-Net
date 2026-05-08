# tools/target_app/CMakeLists.txt
add_executable(target_app target_app.c)
target_link_libraries(target_app ${MOSQUITTO_LIBRARIES})
target_include_directories(target_app PRIVATE ${MOSQUITTO_INCLUDE_DIRS})