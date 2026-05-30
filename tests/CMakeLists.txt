add_executable(test_audiofile test_audiofile.cpp)
target_link_libraries(test_audiofile PRIVATE ldnss_core)
set_target_properties(test_audiofile PROPERTIES AUTOMOC OFF)
add_test(NAME test_audiofile COMMAND test_audiofile)

add_executable(test_loudness test_loudness.cpp)
target_link_libraries(test_loudness PRIVATE ldnss_core)
set_target_properties(test_loudness PROPERTIES AUTOMOC OFF)
add_test(NAME test_loudness COMMAND test_loudness)

add_executable(test_livemeter test_livemeter.cpp)
target_link_libraries(test_livemeter PRIVATE ldnss_core)
set_target_properties(test_livemeter PROPERTIES AUTOMOC OFF)
add_test(NAME test_livemeter COMMAND test_livemeter)
