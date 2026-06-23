#include "ai_sign/FrameSource.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ai_sign {
namespace {

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> cells;
    std::string current;
    bool quoted = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (ch == ',' && !quoted) {
            cells.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    cells.push_back(trim(current));
    return cells;
}

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

} // namespace

CsvFrameSource::CsvFrameSource(std::vector<LandmarkFrame> frames, bool loop)
    : frames_(std::move(frames)), loop_(loop) {}

std::vector<LandmarkFrame> CsvFrameSource::loadFromCsv(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Could not open frame file: " + path.string());
    }

    std::vector<LandmarkFrame> frames;
    std::string line;
    std::size_t row = 0;

    while (std::getline(input, line)) {
        ++row;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const auto cells = splitCsvLine(trimmed);
        if (cells.empty() || cells[0] == "frame_id") {
            continue;
        }
        if (cells.size() < 5) {
            throw std::runtime_error("Frame CSV row " + std::to_string(row) + " has too few columns.");
        }

        LandmarkFrame frame;
        frame.id = cells[0].empty() ? "frame-" + std::to_string(frames.size() + 1) : cells[0];
        frame.sourceLabel = cells[1];

        for (std::size_t i = 2; i < cells.size(); ++i) {
            try {
                frame.features.push_back(std::stod(cells[i]));
            } catch (const std::exception&) {
                throw std::runtime_error("Invalid numeric value in frame CSV row " + std::to_string(row) + ".");
            }
        }

        frames.push_back(std::move(frame));
    }

    if (frames.empty()) {
        throw std::runtime_error("Frame file did not contain any usable frames: " + path.string());
    }

    return frames;
}

std::optional<LandmarkFrame> CsvFrameSource::next() {
    if (frames_.empty()) {
        return std::nullopt;
    }

    if (index_ >= frames_.size()) {
        if (!loop_) {
            return std::nullopt;
        }
        index_ = 0;
    }

    return frames_[index_++];
}

std::string CsvFrameSource::name() const {
    return "CSV live landmark replay";
}

SyntheticFrameSource::SyntheticFrameSource(std::vector<Prototype> prototypes, unsigned seed)
    : prototypes_(std::move(prototypes)), rng_(seed), noise_(0.0, 0.022) {}

std::optional<LandmarkFrame> SyntheticFrameSource::next() {
    if (prototypes_.empty()) {
        return std::nullopt;
    }

    const auto& prototype = prototypes_[(tick_ / holdFrames_) % prototypes_.size()];

    LandmarkFrame frame;
    frame.id = "synthetic-" + std::to_string(tick_ + 1);
    frame.sourceLabel = prototype.label;
    frame.features.reserve(prototype.features.size());

    for (const double value : prototype.features) {
        frame.features.push_back(clamp01(value + noise_(rng_)));
    }

    ++tick_;
    return frame;
}

std::string SyntheticFrameSource::name() const {
    return "Synthetic live landmark stream";
}

} // namespace ai_sign
