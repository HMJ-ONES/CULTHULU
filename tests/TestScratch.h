#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

// Portable, isolated test files. Never assume that /tmp exists or is writable.
class TestScratch {
public:
    explicit TestScratch(const char* label) {
        std::error_code error;
        base_ = std::filesystem::temp_directory_path(error).lexically_normal();
        if (error) return;
        static std::atomic<unsigned long long> sequence{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 16; ++attempt) {
            path_ = base_ / (std::string("cultulhu-") + label + "-" +
                    std::to_string(stamp) + "-" + std::to_string(++sequence));
            if (std::filesystem::create_directory(path_, error)) { owned_ = true; return; }
            if (error) return;
        }
    }
    ~TestScratch() {
        // Only remove the unique directory successfully created by this instance.
        if (owned_ && path_.parent_path().lexically_normal() == base_ &&
            path_.filename().string().rfind("cultulhu-", 0) == 0) {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
    }
    TestScratch(const TestScratch&) = delete;
    TestScratch& operator=(const TestScratch&) = delete;
    bool valid() const { return owned_; }
    std::string file(const char* name) const { return (path_ / name).string(); }
private:
    std::filesystem::path base_, path_;
    bool owned_ = false;
};
