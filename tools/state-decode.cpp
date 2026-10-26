#include "llama.h"
#include "ggml.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

static int argmax(const float* p, int n) {
    int best = 0;
    for (int i = 1; i < n; ++i) if (p[i] > p[best]) best = i;
    return best;
}

int main(int argc, char** argv) {
    if (argc != 6) { std::printf("usage: %s model.gguf state.bin first_token n pos0\n", argv[0]); return 1; }
    const int n_gen = std::atoi(argv[4]);
    const int pos0 = std::atoi(argv[5]);
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 999;
    llama_model* model = llama_model_load_from_file(argv[1], mp);
    if (!model) { std::printf("model load failed\n"); return 2; }

    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = 2048; cp.n_batch = 512; cp.n_ubatch = 512;
    cp.type_k = GGML_TYPE_F32; cp.type_v = GGML_TYPE_F32;
    llama_context* ctx = llama_init_from_model(model, cp);

    std::ifstream f(argv[2], std::ios::binary);
    std::vector<char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (!llama_state_seq_set_data(ctx, (const uint8_t*)buf.data(), buf.size(), 0)) {
        std::printf("state import failed\n"); return 3;
    }

    llama_token cur = (llama_token)std::atoi(argv[3]);
    const int n_vocab = llama_vocab_n_tokens(llama_model_get_vocab(model));
    for (int i = 0; i < n_gen; ++i) {
        llama_batch b = llama_batch_init(1, 0, 1);
        b.token[0] = cur; b.pos[0] = pos0 + i; b.n_seq_id[0] = 1;
        b.seq_id[0][0] = 0; b.logits[0] = 1; b.n_tokens = 1;
        if (llama_decode(ctx, b) != 0) { std::printf("decode fail %d\n", i); return 4; }
        cur = argmax(llama_get_logits_ith(ctx, -1), n_vocab);
        std::printf("%d %d\n", i, cur);
        llama_batch_free(b);
    }
    llama_free(ctx); llama_model_free(model); llama_backend_free();
    return 0;
}
