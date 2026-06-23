#pragma once

#include "ai_sign/Classifier.hpp"

#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace ai_sign {

struct LandmarkFrame {
    std::string id;
    std::string sourceLabel;
    std::vector<double> features;
};

class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    virtual std::optional<LandmarkFrame> next() = 0;
    virtual std::string name() const = 0;
};

class CsvFrameSource final : public IFrameSource {
public:
    CsvFrameSource(std::vector<LandmarkFrame> frames, bool loop = true);

    static std::vector<LandmarkFrame> loadFromCsv(const std::filesystem::path& path);

    std::optional<LandmarkFrame> next() override;
    std::string name() const override;

private:
    std::vector<LandmarkFrame> frames_;
    std::size_t index_ = 0;
    bool loop_ = true;
};

class SyntheticFrameSource final : public IFrameSource {
public:
    explicit SyntheticFrameSource(std::vector<Prototype> prototypes, unsigned seed = 2026);

    std::optional<LandmarkFrame> next() override;
    std::string name() const override;

private:
    std::vector<Prototype> prototypes_;
    std::mt19937 rng_;
    std::normal_distribution<double> noise_;
    std::size_t tick_ = 0;
    std::size_t holdFrames_ = 8;
};

} // namespace ai_sign
