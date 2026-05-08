add_executable(test_logger test_logger.c ../src/logger.c)
target_link_libraries(test_logger pthread)
target_include_directories(test_logger PRIVATE ../include)
add_test(NAME test_logger COMMAND test_logger)

add_executable(test_observer test_observer.c ../src/observer.c ../src/logger.c)
target_link_libraries(test_observer pthread)
target_include_directories(test_observer PRIVATE ../include)
add_test(NAME test_observer COMMAND test_observer)

add_executable(test_injector_net test_injector_net.c ../src/injector_net.c ../src/logger.c)
target_include_directories(test_injector_net PRIVATE ../include)
add_test(NAME test_injector_net COMMAND test_injector_net)

add_executable(test_injector_mem test_injector_mem.c ../src/injector_mem.c ../src/logger.c)
target_link_libraries(test_injector_mem pthread)
target_include_directories(test_injector_mem PRIVATE ../include)
add_test(NAME test_injector_mem COMMAND test_injector_mem)

add_executable(test_scenario test_scenario.c ../src/scenario.c ../src/logger.c ../src/injector_net.c ../src/injector_cpu.c ../src/injector_mem.c)
target_link_libraries(test_scenario pthread)
target_include_directories(test_scenario PRIVATE ../include)
add_test(NAME test_scenario COMMAND test_scenario)
