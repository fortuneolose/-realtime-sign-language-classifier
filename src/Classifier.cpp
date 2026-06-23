#include "ai_sign/Classifier.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <unordered_map>

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

double normalizedDistance(const std::vector<double>& lhs, const std::vector<double>& rhs) {
    const std::size_t shared = std::min(lhs.size(), rhs.size());
    if (shared == 0) {
        return 1.0;
    }

    double sum = 0.0;
    for (std::size_t i = 0; i < shared; ++i) {
        const double delta = lhs[i] - rhs[i];
        sum += delta * delta;
    }

    const double dimensionPenalty =
        0.05 * static_cast<double>(lhs.size() > rhs.size() ? lhs.size() - rhs.size() : rhs.size() - lhs.size());
    return std::sqrt(sum / static_cast<double>(shared)) + dimensionPenalty;
}

} // namespace

NearestPrototypeClassifier::NearestPrototypeClassifier(std::vector<Prototype> prototypes)
    : prototypes_(std::move(prototypes)) {
    if (prototypes_.empty()) {
        prototypes_ = defaults();
    }
}

std::vector<Prototype> NearestPrototypeClassifier::loadFromCsv(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Could not open prototype file: " + path.string());
    }

    std::vector<Prototype> prototypes;
    std::string line;
    std::size_t row = 0;

    while (std::getline(input, line)) {
        ++row;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const auto cells = splitCsvLine(trimmed);
        if (cells.empty() || cells[0] == "label") {
            continue;
        }
        if (cells.size() < 5) {
            throw std::runtime_error("Prototype CSV row " + std::to_string(row) + " has too few columns.");
        }

        Prototype prototype;
        prototype.label = cells[0];
        prototype.display = cells[1].empty() ? cells[0] : cells[1];

        for (std::size_t i = 2; i < cells.size(); ++i) {
            try {
                prototype.features.push_back(std::stod(cells[i]));
            } catch (const std::exception&) {
                throw std::runtime_error("Invalid numeric value in prototype CSV row " + std::to_string(row) + ".");
            }
        }

        prototypes.push_back(std::move(prototype));
    }

    if (prototypes.empty()) {
        throw std::runtime_error("Prototype file did not contain any usable signs: " + path.string());
    }

    return prototypes;
}

std::vector<Prototype> NearestPrototypeClassifier::defaults() {
    return {
        {"HELLO", "Hello", {0.10, 0.88, 0.90, 0.88, 0.86, 0.84, 0.62, 0.70, 0.78, 0.64, 0.72, 0.18}},
        {"YES", "Yes", {0.55, 0.18, 0.14, 0.12, 0.12, 0.10, 0.12, 0.84, 0.44, 0.75, 0.20, 0.88}},
        {"NO", "No", {0.38, 0.28, 0.88, 0.86, 0.18, 0.14, 0.24, 0.48, 0.58, 0.52, 0.28, 0.42}},
        {"THANK_YOU", "Thank You", {0.16, 0.82, 0.92, 0.91, 0.89, 0.88, 0.58, 0.62, 0.64, 0.80, 0.70, 0.16}},
        {"PLEASE", "Please", {0.22, 0.74, 0.86, 0.86, 0.82, 0.80, 0.42, 0.76, 0.48, 0.54, 0.66, 0.22}},
        {"ILY", "I Love You", {0.12, 0.92, 0.94, 0.18, 0.16, 0.91, 0.74, 0.22, 0.68, 0.18, 0.84, 0.50}},
    };
}

Prediction NearestPrototypeClassifier::classify(const std::vector<double>& features) const {
    if (prototypes_.empty() || features.empty()) {
        return {};
    }

    std::vector<Candidate> candidates;
    candidates.reserve(prototypes_.size());
    double scoreTotal = 0.0;

    for (const auto& prototype : prototypes_) {
        const double distance = normalizedDistance(features, prototype.features);
        const double score = std::exp(-6.0 * distance);
        scoreTotal += score;
        candidates.push_back({prototype.label, prototype.display, score, distance});
    }

    if (scoreTotal <= std::numeric_limits<double>::epsilon()) {
        return {};
    }

    for (auto& candidate : candidates) {
        candidate.confidence /= scoreTotal;
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.confidence == rhs.confidence) {
            return lhs.distance < rhs.distance;
        }
        return lhs.confidence > rhs.confidence;
    });

    Prediction prediction;
    prediction.label = candidates.front().label;
    prediction.display = candidates.front().display;
    prediction.confidence = candidates.front().confidence;
    prediction.top = candidates;

    if (prediction.top.size() > 5) {
        prediction.top.resize(5);
    }

    return prediction;
}

const std::vector<Prototype>& NearestPrototypeClassifier::prototypes() const noexcept {
    return prototypes_;
}

PredictionSmoother::PredictionSmoother(std::size_t windowSize)
    : windowSize_(std::max<std::size_t>(1, windowSize)) {}

std::string PredictionSmoother::update(const Prediction& prediction) {
    labels_.push_back(prediction.label.empty() ? "UNKNOWN" : prediction.label);
    while (labels_.size() > windowSize_) {
        labels_.pop_front();
    }

    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& label : labels_) {
        ++counts[label];
    }

    auto best = counts.begin();
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        if (it->second > best->second) {
            best = it;
        }
    }

    return best->first;
}

double PredictionSmoother::stability() const {
    if (labels_.empty()) {
        return 0.0;
    }

    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& label : labels_) {
        ++counts[label];
    }

    std::size_t best = 0;
    for (const auto& entry : counts) {
        best = std::max(best, entry.second);
    }

    return static_cast<double>(best) / static_cast<double>(labels_.size());
}

void PredictionSmoother::reset() {
    labels_.clear();
}

} // namespace ai_sign
