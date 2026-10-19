#include "common.h"
#include "speculative.h"

#include <cstdio>

static int g_failures = 0;

static void expect(const char * name, bool got, bool want) {
    const bool ok = got == want;
    std::printf("%-30s %s (got %d, want %d)\n", name, ok ? "PASS" : "FAIL", (int) got, (int) want);
    if (!ok) {
        ++g_failures;
    }
}

int main() {
    {
        common_batch b;
        for (int i = 0; i < 5; ++i) b.add(1, i, 0, false);
        for (int i = 0; i < 5; ++i) b.add(2, i, 1, false);
        expect("2x5 contiguous", common_speculative_batch_is_contiguous(b, 2), true);
    }
    {
        common_batch b;
        for (int i = 0; i < 4; ++i) b.add(1, i, 0, false);
        b.add(2, 4, 1, false);
        expect("4+1 contiguous", common_speculative_batch_is_contiguous(b, 2), true);
    }
    {
        common_batch b;
        for (int i = 0; i < 3; ++i) b.add(1, i, 0, false);
        for (int i = 0; i < 4; ++i) b.add(2, i, 1, false);
        expect("3+4 contiguous", common_speculative_batch_is_contiguous(b, 2), true);
    }
    {
        common_batch b;
        for (int i = 0; i < 4; ++i) b.add(1, i, i % 2, false);
        expect("interleaved 0/1", common_speculative_batch_is_contiguous(b, 2), false);
    }
    {
        common_batch b;
        for (int i = 0; i < 7; ++i) b.add(1, i, 0, false);
        expect("single sequence", common_speculative_batch_is_contiguous(b, 2), true);
    }
    {
        common_batch b;
        for (int i = 0; i < 3; ++i) b.add(1, i, 0, false);
        b.add(2, 3, 5, false); // seq id out of range
        expect("bad seq id", common_speculative_batch_is_contiguous(b, 2), false);
    }

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
