set(TestCommonSources ${SHADOWER_DIR}/utils.cc ${SHADOWER_DIR}/trace_samples.cc)

add_executable(source_test source_test.cc ${TestCommonSources})
target_link_libraries(source_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(cell_search_test cell_search_test.cc ${TestCommonSources})
target_link_libraries(cell_search_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})
if (USE_CUDA)
  target_link_libraries(cell_search_test ssb_cuda)
endif(USE_CUDA)

add_executable(search_ssb_freq_test search_ssb_freq_test.cc ${SHADOWER_DIR}/syncer.cc ${TestCommonSources})
target_link_libraries(search_ssb_freq_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} Threads::Threads)
if (USE_CUDA)
  target_link_libraries(search_ssb_freq_test ssb_cuda)
endif(USE_CUDA)

add_executable(syncer_test syncer_test.cc ${TestCommonSources} ${SHADOWER_DIR}/syncer.cc)
target_link_libraries(syncer_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} zmq)
if (USE_CUDA)
  target_link_libraries(syncer_test ssb_cuda)
endif(USE_CUDA)

add_executable(sib_search_test sib_search_test.cc ${TestCommonSources})
target_link_libraries(sib_search_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})
if (USE_CUDA)
  target_link_libraries(sib_search_test fft_processor)
endif(USE_CUDA)

add_executable(rach_msg2_test rach_msg2_test.cc ${TestCommonSources})
target_link_libraries(rach_msg2_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(broadcast_worker_test broadcast_worker_test.cc ${TestCommonSources}
    ${SHADOWER_DIR}/broadcast_worker.cc
)
if (USE_CUDA)
  target_link_libraries(broadcast_worker_test fft_processor)
endif(USE_CUDA)

add_executable(rrc_setup_test rrc_setup_test.cc ${TestCommonSources})
target_link_libraries(rrc_setup_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(contention_resolution_test contention_resolution_test.cc ${TestCommonSources})
target_link_libraries(contention_resolution_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(pdsch_decode_test pdsch_decode_test.cc ${TestCommonSources} ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(pdsch_decode_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)
if (USE_CUDA)
  target_link_libraries(pdsch_decode_test fft_processor)
endif(USE_CUDA)

add_executable(pdsch_decode_search pdsch_decode_search.cc ${TestCommonSources} ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(pdsch_decode_search ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

target_link_libraries(broadcast_worker_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(rach_msg3_test rach_msg3_test.cc ${TestCommonSources})
target_link_libraries(rach_msg3_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(pusch_decode_test pusch_decode_test.cc ${TestCommonSources} ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(pusch_decode_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)
if (USE_CUDA)
  target_link_libraries(pusch_decode_test fft_processor)
endif(USE_CUDA)

add_executable(pusch_cfo_test pusch_cfo_test.cc ${TestCommonSources} ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(pusch_cfo_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

add_executable(pdsch_encode_test pdsch_encode_test.cc ${TestCommonSources} ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(pdsch_encode_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

add_executable(dci_ul_encode_test dci_ul_encode_test.cc ${TestCommonSources})
target_link_libraries(dci_ul_encode_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(msg_helper_test msg_helper_test.cc ${TestCommonSources} ${SHADOWER_DIR}/msg_helper.cc ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(msg_helper_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

add_executable(sib_overshadow_test sib_overshadow_test.cc ${TestCommonSources}
    ${SHADOWER_DIR}/syncer.cc
    ${SHADOWER_DIR}/ring_buffer.cc
)
target_link_libraries(sib_overshadow_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} zmq)
if (USE_CUDA)
  target_link_libraries(sib_overshadow_test ssb_cuda)
endif(USE_CUDA)

add_executable(sdr_timing_test sdr_timing_test.cc ${TestCommonSources} ${SHADOWER_DIR}/syncer.cc)
target_link_libraries(sdr_timing_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES})
if (USE_CUDA)
  target_link_libraries(sdr_timing_test ssb_cuda)
endif(USE_CUDA)

add_executable(exploit_rrc_setup_test exploit_rrc_setup_test.cc ${TestCommonSources} ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(exploit_rrc_setup_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

add_executable(rrc_release_construction rrc_release_construction.cc ${TestCommonSources} ${SHADOWER_DIR}/msg_helper.cc ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(rrc_release_construction ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

add_executable(security_mode_command_construction security_mode_command_construction.cc ${TestCommonSources} ${SHADOWER_DIR}/msg_helper.cc ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(security_mode_command_construction ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

add_executable(registration_accept_constructor registration_accept_constructor.cc ${TestCommonSources} ${SHADOWER_DIR}/msg_helper.cc ${SHADOWER_DIR}/wd_worker.cc)
target_link_libraries(registration_accept_constructor ${SRSUE_SOURCES} ${SRSRAN_SOURCES} wdissector)

if (USE_CUDA)
    add_executable(fft_test fft_test.cc ${SHADOWER_DIR}/fft_processor.cu ${TestCommonSources})
    target_link_libraries(fft_test ${SRSUE_SOURCES} ${SRSRAN_SOURCES} ${FFT_LIBRARIES} fft_processor)
endif(USE_CUDA)

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
  add_executable(ldpc_test ldpc_test.cc)
  target_link_libraries(ldpc_test PRIVATE ${SRSUE_SOURCES} ${SRSRAN_SOURCES} srsran_phch srsran_fec)
endif()

add_executable(pcap_writer pcap_writer.cc)
target_link_libraries(pcap_writer ${SRSUE_SOURCES} ${SRSRAN_SOURCES})

add_executable(resampler resampler_test.cc)
target_link_libraries(resampler ${SRSUE_SOURCES} ${SRSRAN_SOURCES} liquid)