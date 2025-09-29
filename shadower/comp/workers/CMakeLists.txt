add_library(broadcast_worker broadcast_worker.cc broadcast_worker.h)
target_link_libraries(broadcast_worker utils srsran_mac)
if(ENABLE_CUDA)
    target_link_libraries(broadcast_worker fft_processor)
endif(ENABLE_CUDA)

add_executable(broadcast_worker_sib1_test tests/broadcast_worker_sib1_test.cc)
target_link_libraries(broadcast_worker_sib1_test broadcast_worker utils)

add_test(shd_broadcast_worker_sib1_test_20MHz
    broadcast_worker_sib1_test -s 23.04 -f 3427.5 -b 3421.92 -i 0 -I 1 -p 51
    -F ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/sib.fc32
    -m ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/mib.raw
)

add_test(shd_broadcast_worker_sib1_test_40MHz
    broadcast_worker_sib1_test -s 46.08 -f 3427.5 -b 3413.28 -i 0 -I 1 -p 106
    -F ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-40MHz/sib.fc32
    -m ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-40MHz/mib.raw
)

add_executable(broadcast_worker_rar_test tests/broadcast_worker_rar_test.cc)
target_link_libraries(broadcast_worker_rar_test broadcast_worker utils)

add_test(shd_broadcast_worker_rar_test_20MHz
    broadcast_worker_rar_test -s 23.04 -f 3427.5 -b 3421.92 -i 10 -I 1 -p 51 -R 267
    -F ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/rar.fc32
    -z ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/sib1.raw -Z 101
    -m ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/mib.raw
)

add_test(shd_broadcast_worker_rar_test_40MHz
    broadcast_worker_rar_test -s 46.08 -f 3427.5 -b 3413.28 -i 10 -I 1 -p 106 -R 267
    -F ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-40MHz/rar.fc32
    -z ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-40MHz/sib1.raw -Z 101
    -m ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-40MHz/mib.raw
)

add_library(wd_worker wd_worker.cc)
target_link_libraries(wd_worker wdissector srslog)
target_include_directories(wd_worker PUBLIC ${WDISSECTOR_PATH}/src ${WDISSECTOR_INCLUDES})

add_executable(wd_worker_test tests/wd_worker_test.cc)
target_link_libraries(wd_worker_test wd_worker utils)

add_library(ue_dl_worker ue_dl_worker.cc)
target_link_libraries(ue_dl_worker utils srsran_mac wd_worker)
target_include_directories(ue_dl_worker PUBLIC ${WDISSECTOR_PATH}/src ${WDISSECTOR_INCLUDES})
if(ENABLE_CUDA)
    target_link_libraries(ue_dl_worker fft_processor)
endif(ENABLE_CUDA)

add_executable(ue_dl_worker_test tests/ue_dl_worker_test.cc)
target_link_libraries(ue_dl_worker_test ue_dl_worker utils)

add_test(shd_ue_dl_worker_test_20MHz
    ${CMAKE_BINARY_DIR}/shadower/comp/workers/ue_dl_worker_test -s 23.04 -f 3427.5 -b 3421.92 -c 1 -I 1 -p 51 -R 17921
    -m ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/mib.raw
    -z ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/sib1.raw -Z 101
    -y ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/rrc_setup.raw -Y 316
    -F ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/pdsch_3684.fc32 -i 4
)
set_tests_properties(shd_ue_dl_worker_test_20MHz PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_library(gnb_ul_worker gnb_ul_worker.cc)
target_link_libraries(gnb_ul_worker utils srsran_mac wd_worker)
target_include_directories(gnb_ul_worker PUBLIC ${WDISSECTOR_PATH}/src ${WDISSECTOR_INCLUDES})
if(ENABLE_CUDA)
    target_link_libraries(gnb_ul_worker fft_processor)
endif(ENABLE_CUDA)

add_executable(gnb_ul_worker_test tests/gnb_ul_worker_test.cc)
target_link_libraries(gnb_ul_worker_test gnb_ul_worker ue_dl_worker utils)

add_test(shd_gnb_ul_worker_test_20MHz
    ${CMAKE_BINARY_DIR}/shadower/comp/workers/gnb_ul_worker_test -s 23.04 -f 3427.5 -b 3421.92 -c 1 -I 1 -p 51 -R 17921
    -m ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/mib.raw
    -z ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/sib1.raw -Z 101
    -y ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/rrc_setup.raw -Y 316
    -d ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/dci_ul_3622.fc32 -D 2
    -F ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/pusch_3626.fc32 -i 6
    -o 468 -u 0.00054
)
set_tests_properties(shd_gnb_ul_worker_test_20MHz PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_library(gnb_dl_worker gnb_dl_worker.cc)
target_link_libraries(gnb_dl_worker utils)

add_executable(gnb_dl_worker_test tests/gnb_dl_worker_test.cc)
target_link_libraries(gnb_dl_worker_test gnb_dl_worker utils source)

add_test(shd_gnb_dl_worker_test_20MHz
    ${CMAKE_BINARY_DIR}/shadower/comp/workers/gnb_dl_worker_test -s 23.04 -f 3427.5 -b 3421.92 -c 1 -I 1 -p 51 -R 17921
    -m ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/mib.raw
    -z ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/sib1.raw -Z 101
    -y ${CMAKE_SOURCE_DIR}/shadower/test/data/srsran-n78-20MHz/rrc_setup.raw -Y 316
    -M 3 -P 24
)
set_tests_properties(shd_gnb_dl_worker_test_20MHz PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_library(ue_ul_worker ue_ul_worker.cc)
target_link_libraries(ue_ul_worker utils)

add_executable(ue_ul_worker_test tests/ue_ul_worker_test.cc)
target_link_libraries(ue_ul_worker_test ue_ul_worker utils source)
