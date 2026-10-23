// strix — single entry point for the strix.cpp toolchain.
//
// Subcommands forward to the sibling executables in the same directory, so the
// upstream binaries keep their names (llama-cli, strix-server, llama-bench,
// llama-quantize, llama-mtmd-cli) and nothing has to be renamed or symlinked.
// `diagnose` and `version` are implemented here.
//
// The two version strings come from common/build-info.cpp, generated from
// version.txt and upstream.txt at configure time.

#include "build-info.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#  include <process.h>
#  define STRIX_ACCESS _access
#  define STRIX_EXECV _execv
#else
#  include <unistd.h>
#  include <limits.h>
#  include <sys/wait.h>
#  define STRIX_ACCESS access
#  define STRIX_EXECV execv
#endif

// Defined in common/build-info.cpp; declared here so this tool does not need
// build-info.h to change on every release.
extern const char * STRIX_RELEASE_VERSION;
extern const char * STRIX_UPSTREAM_BASE;

namespace {

struct Command {
    const char * name;
    const char * binary;
    const char * summary;
};

const Command kCommands[] = {
    { "serve",    "strix-server",   "OpenAI-compatible server" },
    { "run",      "llama-cli",      "interactive / one-shot generation" },
    { "bench",    "llama-bench",    "benchmark (llama-bench)" },
    { "quantize", "llama-quantize", "quantize a GGUF model" },
    { "mtmd",     "llama-mtmd-cli", "multimodal CLI" },
};

std::string self_dir() {
#if defined(_WIN32)
    return ".";
#else
    char buf[PATH_MAX];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    std::string path(buf);
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
#endif
}

std::string capture(const char * cmd) {
    std::string out;
    FILE * pipe = ::popen(cmd, "r");
    if (!pipe) {
        return out;
    }
    char buf[512];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        out += buf;
    }
    ::pclose(pipe);
    return out;
}

bool file_exists(const std::string & path) {
    return STRIX_ACCESS(path.c_str(), 0) == 0;
}

void print_usage(FILE * out) {
    std::fprintf(out,
        "strix.cpp %s (upstream base %s)\n"
        "\n"
        "usage: strix <command> [options...]\n"
        "\n"
        "commands:\n",
        STRIX_RELEASE_VERSION, STRIX_UPSTREAM_BASE);
    for (const auto & c : kCommands) {
        std::fprintf(out, "  %-9s %s\n", c.name, c.summary);
    }
    std::fprintf(out,
        "  %-9s %s\n"
        "  %-9s %s\n"
        "  %-9s %s\n"
        "\n"
        "Each command forwards its remaining arguments to the matching binary in\n"
        "this directory; `strix <command> --help` shows that tool's own help.\n",
        "diagnose", "check ROCm, GPU and model availability",
        "version",  "print version and upstream base",
        "help",     "this message");
}

int diagnose(const std::vector<std::string> & args) {
    std::string model;
    for (size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "-m" || args[i] == "--model") && i + 1 < args.size()) {
            model = args[i + 1];
        }
    }

    std::printf("strix.cpp %s (upstream base %s)\n", STRIX_RELEASE_VERSION, STRIX_UPSTREAM_BASE);
    std::printf("build:    %s\n", llama_build_info());
    std::printf("compiler: %s\n", llama_compiler());

    std::printf("\nROCm\n");
    const char * rocm_env = std::getenv("ROCM_PATH");
    const std::string rocm = rocm_env ? rocm_env : "/opt/rocm";
    std::printf("  path:     %s%s\n", rocm.c_str(), file_exists(rocm) ? "" : "  (missing)");
    const std::string version_file = rocm + "/.info/version";
    if (file_exists(version_file)) {
        FILE * f = std::fopen(version_file.c_str(), "r");
        char buf[128] = {0};
        if (f) {
            if (std::fgets(buf, sizeof(buf), f)) {
                buf[std::strcspn(buf, "\n")] = '\0';
                std::printf("  version:  %s\n", buf);
            }
            std::fclose(f);
        }
    } else {
        std::printf("  version:  unknown (%s not found)\n", version_file.c_str());
    }

    const char * hsa = std::getenv("HSA_OVERRIDE_GFX_VERSION");
    std::printf("  HSA_OVERRIDE_GFX_VERSION: %s\n", hsa ? hsa : "(unset)");

    std::printf("\nGPU\n");
    const std::string agents = capture("rocminfo 2>/dev/null | grep -E 'Name:|gfx[0-9]+' | head -8");
    if (agents.empty()) {
        std::printf("  rocminfo: unavailable or no agents\n");
    } else {
        std::printf("%s", agents.c_str());
    }

    std::printf("\nBinaries (this directory)\n");
    const std::string dir = self_dir();
    for (const auto & c : kCommands) {
        const std::string path = dir + "/" + c.binary;
        std::printf("  %-18s %s\n", c.binary, file_exists(path) ? "ok" : "missing");
    }

    std::printf("\nModel\n");
    if (model.empty()) {
        std::printf("  (pass -m <model.gguf> to check a file)\n");
    } else if (!file_exists(model)) {
        std::printf("  %s: not found\n", model.c_str());
    } else {
        FILE * f = std::fopen(model.c_str(), "rb");
        unsigned char magic[4] = {0};
        if (f) {
            std::fread(magic, 1, sizeof(magic), f);
            std::fclose(f);
        }
        const bool gguf = std::memcmp(magic, "GGUF", 4) == 0;
        std::printf("  %s: %s\n", model.c_str(), gguf ? "GGUF" : "not a GGUF file");
    }
    return 0;
}

int forward(const std::string & binary, int argc, char ** argv) {
    const std::string path = self_dir() + "/" + binary;
    if (file_exists(path)) {
        argv[0] = const_cast<char *>(path.c_str());
        STRIX_EXECV(path.c_str(), argv);
        std::perror(path.c_str());
        return 127;
    }
#if !defined(_WIN32)
    argv[0] = const_cast<char *>(binary.c_str());
    ::execvp(binary.c_str(), argv);
    std::perror(binary.c_str());
#else
    std::fprintf(stderr, "strix: %s not found next to this executable\n", binary.c_str());
#endif
    return 127;
}

} // namespace

int main(int argc, char ** argv) {
    if (argc < 2) {
        print_usage(stderr);
        return 1;
    }

    const std::string cmd = argv[1];
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_usage(stdout);
        return 0;
    }
    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        std::printf("strix.cpp %s (upstream base %s)\n", STRIX_RELEASE_VERSION, STRIX_UPSTREAM_BASE);
        return 0;
    }
    if (cmd == "diagnose") {
        return diagnose(std::vector<std::string>(argv + 2, argv + argc));
    }
    for (const auto & c : kCommands) {
        if (cmd == c.name) {
            return forward(c.binary, argc - 1, argv + 1);
        }
    }

    std::fprintf(stderr, "strix: unknown command '%s'\n\n", cmd.c_str());
    print_usage(stderr);
    return 1;
}
