# strix.cpp own code. Everything under this directory is maintained here;
# upstream directories (ggml/, src/, tools/, common/, examples/) stay untouched
# so that upstream syncs keep producing predictable conflicts.

add_executable(strix cli/main.cpp)
target_link_libraries(strix PRIVATE llama-common-base)
target_compile_features(strix PRIVATE cxx_std_17)

if (LLAMA_TOOLS_INSTALL)
    install(TARGETS strix RUNTIME)
endif()
