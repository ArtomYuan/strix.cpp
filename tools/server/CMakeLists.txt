include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# server-context containing the core server logic, used by strix-server and CLI

set(TARGET server-context)

add_library(${TARGET} STATIC
    server-chat.cpp
    server-chat.h
    server-task.cpp
    server-task.h
    server-queue.cpp
    server-queue.h
    server-common.cpp
    server-common.h
    server-decision.cpp
    server-decision.h
    server-context.cpp
    server-context.h
    server-stream.cpp
    server-stream.h
    server-tools.cpp
    server-tools.h
    server-mcp.cpp
    server-mcp.h
    server-schema.cpp
    server-schema.h
)

if (BUILD_SHARED_LIBS)
    set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(${TARGET} PRIVATE ../mtmd)
target_include_directories(${TARGET} PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(${TARGET} PUBLIC llama-common mtmd ${CMAKE_THREAD_LIBS_INIT})

# strix-server-impl: server logic, reusable by app

set(TARGET strix-server-impl)

add_library(${TARGET}
    server.cpp
    server-http.cpp
    server-http.h
    server-models.cpp
    server-models.h
)
set_target_properties(${TARGET} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${TARGET} PRIVATE ../mtmd ${PROJECT_SOURCE_DIR})
target_link_libraries(${TARGET} PUBLIC server-context llama-ui cpp-httplib ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(${TARGET} llama-ui-assets)

if(LLAMA_TOOLS_INSTALL)
    install(TARGETS ${TARGET} LIBRARY)
endif()

# strix-server executable

set(TARGET strix-server)

add_executable(${TARGET} main.cpp)
install(TARGETS ${TARGET} RUNTIME)

target_link_libraries(${TARGET} PRIVATE strix-server-impl)
target_compile_features(${TARGET} PRIVATE cxx_std_17)

# compatibility alias: upstream scripts and old commands still call llama-server
if (UNIX)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f $<TARGET_FILE_DIR:${TARGET}>/llama-server
        COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/llama-server
        COMMENT "Creating llama-server compatibility symlink"
        VERBATIM)
endif()
