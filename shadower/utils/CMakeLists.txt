find_package(yaml-cpp REQUIRED)

add_library(utils
    src/arg_parser.cc
    src/utils.cc
    src/ssb_utils.cc
    src/phy_cfg_utils.cc
    src/ue_dl_utils.cc
    src/gnb_ul_utils.cc
    src/gnb_dl_utils.cc
    src/ue_ul_utils.cc
    src/dci_utils.cc
    msg_helper.cc
)
target_link_libraries(utils ${SRSUE_SOURCES} ${SRSRAN_SOURCES} yaml-cpp)

add_library(thread_pool thread_pool.cc)
target_link_libraries(thread_pool Threads::Threads utils)
