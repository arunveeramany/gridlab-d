SET(GLD_MODULE_NAME tape)

if (GLD_USE_HDF5)
    FIND_PACKAGE(HDF5 REQUIRED COMPONENTS CXX)
endif ()

add_library(${GLD_MODULE_NAME} SHARED
        collector.cpp
        collector.h
        file.cpp
        file.h
        group_recorder.h
        group_recorder.cpp
        metrics_collector.cpp
        metrics_collector.h
        metrics_collector_writer.cpp
        metrics_collector_writer.h
        violation_recorder.h
        violation_recorder.cpp
        histogram.cpp
        histogram.h
        loadshape.cpp
        loadshape.h
        memory.cpp
        memory.h
        multi_recorder.cpp
        multi_recorder.h
        player.cpp
        player.h
        recorder.cpp
        recorder.h
        shaper.cpp
        shaper.h
        tape.cpp
        tape.h
)

target_compile_options(${GLD_MODULE_NAME} PRIVATE ${GLD_COMPILE_OPTIONS})
set_target_properties(${GLD_MODULE_NAME} PROPERTIES PREFIX "")
target_include_directories(${GLD_MODULE_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/gldcore")

# Link to core library + OS libs (do not link to gridlabd executable)
target_link_libraries(${GLD_MODULE_NAME} PRIVATE gldapi ${OS_SPECIFIC_LIBRARIES})
target_link_libraries(${GLD_MODULE_NAME} PRIVATE nlohmann_json::nlohmann_json)

if (GLD_USE_EIGEN)
    target_include_directories(${GLD_MODULE_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/third_party/Eigen")
    target_link_libraries(${GLD_MODULE_NAME} PRIVATE Eigen3::Eigen)
else ()
    target_link_libraries(${GLD_MODULE_NAME} PRIVATE ${SUPERLU_LIB})
endif ()

if (HDF5_FOUND)
    target_link_libraries(${GLD_MODULE_NAME} PUBLIC ${HDF5_LIBRARIES})
    target_include_directories(${GLD_MODULE_NAME} PUBLIC ${HDF5_INCLUDE_DIRS})
    target_compile_definitions(${GLD_MODULE_NAME} PRIVATE HAVE_HDF5)
endif ()

if(APPLE)
    # Prefer late-bound plugin symbol resolution on macOS
    target_link_options(${GLD_MODULE_NAME} PRIVATE -Wl,-undefined,dynamic_lookup)
endif ()

if (GLD_USE_CLANG_TIDY)
    set_target_properties(${GLD_MODULE_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    )
endif ()
