#pragma once
#include "common.h"
#include "memory_policy.hpp"
#include "stb/stb_image.h"
#include "gguf.h"
#include "mtmd-helper.h"
#include "mtmd.h"
#include <array>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace strata_frontend {
// Private per-request file, retained through streaming completion/cancellation.
struct ImageFile {
    std::string path;
    ImageFile() {
        char name[] = "/tmp/unbiased-strata-image-XXXXXX";
        int fd = mkstemp(name);
        if (fd < 0)
            throw std::runtime_error("cannot create image embedding file");
        close(fd);
        path = name;
    }
    ~ImageFile() {
        if (!path.empty())
            unlink(path.c_str());
    }
    ImageFile(const ImageFile &) = delete;
    ImageFile &operator=(const ImageFile &) = delete;
};

inline std::vector<unsigned char> image_data(const std::string &url) {
    const size_t comma = url.find(',');
    if (url.rfind("data:image/", 0) != 0 || comma == std::string::npos || comma < 7 ||
        url.substr(comma - 7, 7) != ";base64")
        throw std::invalid_argument("image_url must contain a base64 data:image URL (PNG, JPEG, BMP or GIF)");
    const std::string data = url.substr(comma + 1);
    if (data.empty() || data.size() > 32 * 1024 * 1024 || data.size() % 4)
        throw std::invalid_argument("empty, oversized or malformed base64 image");
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::array<int, 256> table;
    table.fill(-1);
    for (size_t i = 0; i < alphabet.size(); ++i)
        table[(unsigned char)alphabet[i]] = (int)i;
    std::vector<unsigned char> out;
    out.reserve(data.size() / 4 * 3);
    for (size_t i = 0; i < data.size(); i += 4) {
        int a = table[(unsigned char)data[i]], b = table[(unsigned char)data[i + 1]];
        bool p2 = data[i + 2] == '=', p3 = data[i + 3] == '=';
        int c = p2 ? 0 : table[(unsigned char)data[i + 2]], d = p3 ? 0 : table[(unsigned char)data[i + 3]];
        if (a < 0 || b < 0 || c < 0 || d < 0 || (p2 && !p3) || ((p2 || p3) && i + 4 != data.size()) ||
            (p2 && (b & 15)) || (!p2 && p3 && (c & 3)))
            throw std::invalid_argument("malformed base64 image");
        out.push_back((a << 2) | (b >> 4));
        if (!p2)
            out.push_back((b << 4) | (c >> 2));
        if (!p3)
            out.push_back((c << 6) | d);
    }
    return out;
}
struct EncodedImage {
    int32_t tokens, nx, ny, width;
    std::vector<float> values;
};

// The picture reader has two implementations behind one interface:
//  * in-process llama.cpp mtmd on the CPU - the historical path, used when the profile does not ask for the
//    card and as the fallback when the card reader will not start;
//  * the engine folder's card reader (`strata-vision-hip`, the same program built against llama.cpp's HIP
//    backend) spawned per picture when the profile sets vision.gpu. It answers in seconds where the CPU path
//    needs tens of seconds, and the projector never enters this process.
// Callers take engine.mu, serializing image encoding and generation just like upstream.
class Vision {
    mtmd_context *ctx_ = nullptr;
    int width_ = 0;
    int max_tokens_ = 1024;
    int threads_ = 8;
    bool gpu_ = false;                          // the card reader is what answers
    std::string reader_;                        // card reader executable; empty = in-process encoder
    std::string mmproj_, model_path_;
    const llama_model *model_ = nullptr;
    size_t cache_bytes_ = 0;
    std::map<std::string, std::shared_ptr<EncodedImage>> cache_;

  public:
    ~Vision() {
        if (ctx_)
            mtmd_free(ctx_);
    }
    bool enabled() const { return ctx_ != nullptr || !reader_.empty(); }

    // exe is the profile's own CPU reader path (strata-vision); the card reader is looked for beside it.
    void init(const std::string &path, const llama_model *model, const std::string &model_path,
              const std::string &exe, int threads, int max_tokens, bool gpu = false) {
        model_ = model;
        model_path_ = model_path;
        mmproj_ = path;
        max_tokens_ = max_tokens;
        threads_ = threads > 0 ? threads : 8;
        // The projector must produce rows of the model's own width (2560 for Flash-Next, 2048 for
        // Qwen3.6-35B-A3B); the tokenizer-only model the front end holds has no hparams, so the width is read
        // from its metadata. Checked before either reader starts, because a mismatch answers nonsense.
        gguf_init_params gp{true, nullptr};
        auto gg = std::unique_ptr<gguf_context, decltype(&gguf_free)>(gguf_init_from_file(path.c_str(), gp),
                                                                      gguf_free);
        auto key = gg ? gguf_find_key(gg.get(), "clip.vision.projection_dim") : -1;
        if (key < 0)
            throw std::runtime_error("vision encoder has no projection_dim");
        width_ = (int)gguf_get_val_u32(gg.get(), key);
        char arch[64] = {0}, val[32] = {0};
        int want = 0;
        if (llama_model_meta_val_str(model, "general.architecture", arch, sizeof arch) > 0 &&
            llama_model_meta_val_str(model, (std::string(arch) + ".embedding_length").c_str(), val,
                                     sizeof val) > 0)
            want = std::atoi(val);
        if (want <= 0)
            throw std::runtime_error("cannot read the model's embedding width");
        if (width_ != want)
            throw std::runtime_error("vision projector width " + std::to_string(width_) +
                                     " does not match the model's embedding width " + std::to_string(want));
        if (gpu) {
            auto dir = std::filesystem::path(exe).parent_path();
            auto candidate = dir / "strata-vision-hip";
            std::error_code ec;
            if (!exe.empty() && std::filesystem::is_regular_file(candidate, ec) &&
                access(candidate.c_str(), X_OK) == 0) {
                reader_ = candidate.string();
                gpu_ = true;
                std::fprintf(stderr, "strata-native: pictures use the GPU reader %s\n", reader_.c_str());
                return;
            }
            std::fprintf(stderr,
                         "strata-native: vision.gpu is set but %s is missing, using the CPU encoder\n",
                         candidate.c_str());
        }
        start_cpu();
    }

    std::shared_ptr<EncodedImage> encode(const std::string &url) {
        if (!enabled())
            throw std::invalid_argument("image input requires a vision profile or --mmproj PATH");
        auto bytes = image_data(url);
        int pixel_width = 0, pixel_height = 0, channels = 0;
        if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                   &pixel_width, &pixel_height, &channels) ||
            pixel_width <= 0 || pixel_height <= 0 ||
            static_cast<uint64_t>(pixel_width) * pixel_height > 40ULL * 1024 * 1024)
            throw std::invalid_argument("Image dimensions invalid or exceed 40 megapixels; use PNG, JPEG, BMP or GIF");
        const auto admission = native_memory::refusal(native_memory::snapshot(), 4 * native_memory::GiB, 0,
            static_cast<uint64_t>(pixel_width) * pixel_height * 16 + 256 * native_memory::MiB);
        if (!admission.empty()) throw std::runtime_error(admission);
        auto t0 = std::chrono::steady_clock::now();
        const std::string key = content_key(bytes);
        auto cached = cache_.find(key);
        if (cached != cache_.end())
            return cached->second;
        if (!reader_.empty()) {
            try {
                return keep(key, encode_reader(bytes, t0));
            } catch (const std::exception &e) {
                // The card reader is an offer, not a promise: a build without the HIP runtime, a card the
                // build has no code for, or a full card falls back to the CPU encoder with one line.
                std::fprintf(stderr, "strata-native: GPU vision failed (%s), using the CPU encoder\n", e.what());
                reader_.clear();
                gpu_ = false;
                start_cpu();
            }
        }
        return keep(key, encode_cpu(bytes, t0));
    }

  private:
    std::shared_ptr<EncodedImage> keep(const std::string &key, std::shared_ptr<EncodedImage> result) {
        if (!result)
            return result;
        size_t size = result->values.size() * sizeof(float);
        if (cache_bytes_ + size > 128 * 1024 * 1024) {
            cache_.clear();
            cache_bytes_ = 0;
        }
        cache_[key] = result;
        cache_bytes_ += size;
        return result;
    }
    void start_cpu() {
        auto cp = mtmd_context_params_default();
        cp.use_gpu = false;
        cp.print_timings = false;
        cp.warmup = false;
        cp.n_threads = threads_;
        cp.image_max_tokens = max_tokens_;
        ctx_ = mtmd_init_from_file(mmproj_.c_str(), model_, cp);
        if (!ctx_ || !mtmd_support_vision(ctx_))
            throw std::runtime_error("cannot load vision encoder: " + mmproj_);
    }
    // FNV-1a over the picture's bytes: a conversation that sends the same screenshot again (most clients do,
    // every turn) encodes it once, which is what the Python server's reader cache does.
    static std::string content_key(const std::vector<unsigned char> &bytes) {
        uint64_t h = 1469598103934665603ull;
        for (unsigned char b : bytes) {
            h ^= b;
            h *= 1099511628211ull;
        }
        char out[32];
        std::snprintf(out, sizeof out, "%016llx", (unsigned long long)h);
        return out;
    }
    std::shared_ptr<EncodedImage> encode_reader(const std::vector<unsigned char> &bytes,
                                                const std::chrono::steady_clock::time_point &t0) {
        ImageFile image_file, out_file;
        {
            auto f = std::unique_ptr<FILE, decltype(&std::fclose)>(
                std::fopen(image_file.path.c_str(), "wb"), std::fclose);
            if (!f || std::fwrite(bytes.data(), 1, bytes.size(), f.get()) != bytes.size())
                throw std::runtime_error("cannot write the picture for the reader");
        }
        int fds[2];
        if (pipe(fds))
            throw std::runtime_error("cannot start the GPU reader");
        const pid_t pid = fork();
        if (pid < 0) {
            close(fds[0]);
            close(fds[1]);
            throw std::runtime_error("cannot start the GPU reader");
        }
        if (pid == 0) {
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);
            close(fds[1]);
            const std::string threads = std::to_string(threads_);
            const std::string tokens = std::to_string(max_tokens_);
            execl(reader_.c_str(), reader_.c_str(), "--mmproj", mmproj_.c_str(), "--model",
                  model_path_.c_str(), "--gpu", "--threads", threads.c_str(), "--max-tokens",
                  tokens.c_str(), static_cast<char *>(nullptr));
            _exit(127);
        }
        close(fds[0]);
        const std::string line = "ENC " + image_file.path + " " + out_file.path + "\n";
        if (write(fds[1], line.data(), line.size()) != (ssize_t)line.size())
            std::fprintf(stderr, "strata-native: GPU reader did not take the request\n");
        close(fds[1]);
        int status = 0;
        bool finished = false;
        for (int i = 0; i < 15000 && !finished; ++i) {
            const pid_t done = waitpid(pid, &status, WNOHANG);
            if (done == pid)
                finished = true;
            else if (done < 0)
                break;
            else
                usleep(20000);
        }
        if (!finished) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            throw std::runtime_error("GPU reader timed out");
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            throw std::runtime_error("GPU reader exited with status " + std::to_string(
                WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status)));
        std::ifstream in(out_file.path, std::ios::binary);
        int32_t hdr[5] = {0, 0, 0, 0, 0};
        if (!in.read(reinterpret_cast<char *>(hdr), sizeof hdr) || hdr[0] != 0x31455653)
            throw std::runtime_error("GPU reader wrote no image embeddings");
        if (hdr[4] != width_)
            throw std::runtime_error("GPU reader embedding width " + std::to_string(hdr[4]) +
                                     " does not match the projector width " + std::to_string(width_));
        if (hdr[1] <= 0 || hdr[1] > max_tokens_ || (int64_t)hdr[2] * hdr[3] != hdr[1])
            throw std::runtime_error("GPU reader returned an unusable image grid");
        auto result = std::make_shared<EncodedImage>();
        result->tokens = hdr[1];
        result->nx = hdr[2];
        result->ny = hdr[3];
        result->width = hdr[4];
        result->values.resize((size_t)hdr[1] * width_);
        if (!in.read(reinterpret_cast<char *>(result->values.data()),
                     (std::streamsize)(result->values.size() * sizeof(float))))
            throw std::runtime_error("GPU reader wrote short image embeddings");
        log(t0, *result, "GPU");
        return result;
    }
    std::shared_ptr<EncodedImage> encode_cpu(const std::vector<unsigned char> &bytes,
                                             const std::chrono::steady_clock::time_point &t0) {
        auto bw = mtmd_helper_bitmap_init_from_buf(ctx_, bytes.data(), bytes.size(), false,
                                                   mtmd_helper_init_opt_default());
        std::unique_ptr<mtmd_bitmap, decltype(&mtmd_bitmap_free)> bitmap(bw.bitmap, mtmd_bitmap_free);
        if (bw.video_ctx)
            mtmd_helper_video_free(bw.video_ctx);
        if (!bitmap || mtmd_bitmap_is_audio(bitmap.get()))
            throw std::invalid_argument("cannot decode image; use PNG, JPEG, BMP or GIF");
        auto chunks = std::unique_ptr<mtmd_input_chunks, decltype(&mtmd_input_chunks_free)>(
            mtmd_input_chunks_init(), mtmd_input_chunks_free);
        std::string marker = mtmd_default_marker();
        mtmd_input_text txt{marker.c_str(), marker.size(), false, true};
        const mtmd_bitmap *bm[] = {bitmap.get()};
        if (mtmd_tokenize(ctx_, chunks.get(), &txt, bm, 1))
            throw std::invalid_argument("image preprocessing failed");
        const mtmd_input_chunk *image = nullptr;
        for (size_t i = 0; i < mtmd_input_chunks_size(chunks.get()); ++i) {
            auto ch = mtmd_input_chunks_get(chunks.get(), i);
            if (mtmd_input_chunk_get_type(ch) == MTMD_INPUT_CHUNK_TYPE_IMAGE) {
                if (image)
                    throw std::invalid_argument("multiple image chunks are unsupported for this model");
                image = ch;
            }
        }
        if (!image)
            throw std::invalid_argument("no image tokens produced");
        size_t n = mtmd_input_chunk_get_n_tokens(image);
        if (!n || n > (size_t)max_tokens_)
            throw std::invalid_argument("image token count exceeds configured limit");
        if (mtmd_encode_chunk(ctx_, image))
            throw std::runtime_error("vision encoding failed");
        auto last = mtmd_image_tokens_get_decoder_pos(mtmd_input_chunk_get_tokens_image(image), 0, n - 1);
        int nx = (int)last.x + 1, ny = (int)last.y + 1;
        if ((size_t)nx * ny != n)
            throw std::runtime_error("image grid is not rectangular");
        auto result = std::make_shared<EncodedImage>();
        result->tokens = (int32_t)n;
        result->nx = nx;
        result->ny = ny;
        result->width = width_;
        auto values = mtmd_get_output_embd(ctx_);
        if (!values)
            throw std::runtime_error("encoder returned no image embeddings");
        result->values.assign(values, values + n * width_);
        log(t0, *result, "CPU");
        return result;
    }
    static void log(const std::chrono::steady_clock::time_point &t0, const EncodedImage &result,
                    const char *where) {
        auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::fprintf(stderr, "strata-native: encoded image %.0f ms, %d tokens (%dx%d), %s\n", ms,
                     result.tokens, result.nx, result.ny, where);
    }
};
inline void write_images(const ImageFile &file, const std::vector<std::shared_ptr<EncodedImage>> &images) {
    auto f = std::unique_ptr<FILE, decltype(&std::fclose)>(std::fopen(file.path.c_str(), "wb"), std::fclose);
    if (!f)
        throw std::runtime_error("cannot open image embedding file");
    for (auto &im : images) {
        int32_t hdr[] = {0x31455653, im->tokens, im->nx, im->ny, im->width};
        if (std::fwrite(hdr, sizeof(hdr), 1, f.get()) != 1 ||
            std::fwrite(im->values.data(), sizeof(float), im->values.size(), f.get()) != im->values.size())
            throw std::runtime_error("short write to image embedding file");
    }
    if (std::fflush(f.get()))
        throw std::runtime_error("cannot flush image embedding file");
}
} // namespace strata_frontend
