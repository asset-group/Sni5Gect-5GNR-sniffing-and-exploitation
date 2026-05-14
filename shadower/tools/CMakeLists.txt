add_executable(recorder ${SHADOWER_DIR}/tools/recorder.cc ${SHADOWER_DIR}/source/source.cc)
target_link_libraries(recorder pthread liquid)

add_executable(cutter ${SHADOWER_DIR}/tools/cutter.cc)

add_executable(resampler ${SHADOWER_DIR}/tools/resampler.cc)
target_link_libraries(resampler liquid)

add_executable(scanner ${SHADOWER_DIR}/tools/scanner.cc)
target_link_libraries(scanner utils source syncer broadcast_worker)

add_executable(pcap_writer ${SHADOWER_DIR}/tools/pcap_writer.cc)
target_link_libraries(pcap_writer utils)
