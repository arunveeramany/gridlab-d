SET(GLD_MODULE_NAME gridlabd)

#set(CMAKE_BUILD_TYPE Debug) # keep your top-level choice if multi-config
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# ---- Core sources (the "one true core") ----
# IMPORTANT: main.cpp must NOT be compiled into the library if you want to avoid duplicate state
# and keep gridlabd as a thin wrapper.
set(GLDCORE_SOURCE_LIST
        gridlabd.h
        gldapi.h
        gldapi.cpp
        globals.cpp
        globals.h
        object.cpp
        object.h
        schedule.cpp
        schedule.h
        aggregate.cpp
        aggregate.h
        class.cpp
        class.h
        cmdarg.cpp
        cmdarg.h
        compare.cpp
        compare.h
        gld_complex.h
        console.h
        convert.cpp
        convert.h
        cpp_threadpool.cpp
        cpp_threadpool.h
        debug.cpp
        debug.h
        deltamode.cpp
        deltamode.h
        enduse.cpp
        enduse.h
        environment.cpp
        environment.h
        exception.cpp
        exception.h
        exec.cpp
        exec.h
        find.cpp
        find.h
        gld_sock.h
        gui.cpp
        gui.h
        http_client.cpp
        http_client.h
        index.cpp
        index.h
        instance.cpp
        instance.h
        instance_cnx.cpp
        instance_cnx.h
        instance_slave.cpp
        instance_slave.h
        interpolate.cpp
        interpolate.h
        job.cpp
        job.h
        kill.cpp
        kill.h
        kml.cpp
        kml.h
        legal.cpp
        legal.h
        link.cpp
        link.h
        linkage.cpp
        linkage.h
        list.cpp
        list.h
        load.cpp
        load.h
        load_xml.cpp
        load_xml.h
        load_xml_handle.cpp
        load_xml_handle.h
        loadshape.cpp
        loadshape.h
        local.cpp
        local.h
        lock.cpp
        lock.h
        match.cpp
        match.h
        matlab.cpp
        matlab.h
        module.cpp
        module.h
        output.cpp
        output.h
        platform.h
        property.cpp
        property.h
        random.cpp
        gldrandom.h
        realtime.cpp
        realtime.h
        sanitize.cpp
        sanitize.h
        save.cpp
        save.h
        server.cpp
        server.h
        setup.cpp
        setup.h
        stream.cpp
        stream.h
        stream_type.h
        test.cpp
        test.h
        test_callbacks.h
        test_framework.cpp
        test_framework.h
        threadpool.cpp
        threadpool.h
        timestamp.cpp
        timestamp.h
        transform.cpp
        transform.h
        ufile.cpp
        ufile.h
        unit.cpp
        unit.h
        validate.cpp
        validate.h
        version.cpp
)

# -------------------------------------------------------------------------
# 1) Build reusable API library (this owns globals/callback/class registry)
# -------------------------------------------------------------------------
add_library(gldapi SHARED ${GLDCORE_SOURCE_LIST})

# Preserve your existing link behavior
target_link_libraries(gldapi PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(gldapi PRIVATE ${OS_SPECIFIC_LIBRARIES})
target_link_libraries(gldapi PRIVATE ${CMAKE_THREAD_LIBS_INIT})

if (CURSES_FOUND)
    target_link_libraries(gldapi PRIVATE ${CURSES_LIBRARIES})
endif ()

if (XercesC_FOUND)
    target_link_libraries(gldapi PRIVATE ${XercesC_LIBRARIES})
endif ()

# include dirs (keep your generated headers behavior)
target_include_directories(gldapi PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/headers)

# -------------------------------------------------------------------------
# 2) Build executable as thin wrapper (only main.cpp)
# -------------------------------------------------------------------------
add_executable(${GLD_MODULE_NAME} main.cpp)

if(APPLE)
    set_target_properties(gridlabd PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # Keep this: needed so runtime-loaded modules can resolve symbols from the process
    set_target_properties(gridlabd PROPERTIES ENABLE_EXPORTS ON)
endif ()

# Keep the module linkage you had (if GL_MODULES is a list of module targets)
# If GL_MODULES are runtime-loaded plugins, you may not want to link them here;
# but this preserves existing behavior.
target_link_libraries(${GLD_MODULE_NAME} PUBLIC ${GL_MODULES})

# Link the executable to the core library (this is the important change)
target_link_libraries(${GLD_MODULE_NAME} PRIVATE gldapi)

target_link_libraries(${GLD_MODULE_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT})

if (CURSES_FOUND)
    target_link_libraries(${GLD_MODULE_NAME} PRIVATE ${CURSES_LIBRARIES})
endif ()

if (XercesC_FOUND)
    target_link_libraries(${GLD_MODULE_NAME} PRIVATE ${XercesC_LIBRARIES})
endif ()

target_link_libraries(${GLD_MODULE_NAME} PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(${GLD_MODULE_NAME} PRIVATE ${OS_SPECIFIC_LIBRARIES})

# Keep your compile options/debug flags behavior
if(MSVC)
    target_link_options(gridlabd PRIVATE /STACK:16777216)
    message(STATUS "MSVC build detected: Increasing stack size for 'gridlabd' to 16MB to support complex models.")
    target_compile_options(gridlabd PRIVATE $<$<CONFIG:Debug>:/Zi /Od>)
    set_property(TARGET ${GLD_MODULE_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
else()
    target_compile_options(gridlabd PRIVATE $<$<CONFIG:Debug>:-g -O0>)
endif()

target_compile_options(${GLD_MODULE_NAME} PRIVATE ${GLD_COMPILE_OPTIONS})

set_target_properties(${GLD_MODULE_NAME} PROPERTIES PREFIX "")
target_include_directories(${GLD_MODULE_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/headers)

install(TARGETS ${GLD_MODULE_NAME}
        ${FILE_PERMISSIONS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
)

if (GLD_USE_CLANG_TIDY)
    set_target_properties(${GLD_MODULE_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    )
endif ()
