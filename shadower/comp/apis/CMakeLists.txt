add_library(apis apis.cc)
target_link_libraries(apis ${ZEROMQ_LIBRARIES})

add_executable(api_sender_test tests/api_sender_test.cc)
target_link_libraries(api_sender_test apis utils)