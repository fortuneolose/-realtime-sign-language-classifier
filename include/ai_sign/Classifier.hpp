#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace ai_sign {

struct Prototype {
    std::string label;
    std::string display;
    std::vector<double> features;
};

struct Candidate {
    std::string label;
    std::string display;
    double confidence = 0.0;
    double distance = 0.0;
};

struct Prediction {
    std::string label = "UNKNOWN";
    std::string display = "Unknown";
    double confidence = 0.0;
    std::vector<Candidate> top;
};

class NearestPrototypeClassifier {
public:
    explicit NearestPrototypeClassifier(std::vector<Prototype> prototypes);

    static std::vector<Prototype> loadFromCsv(const std::filesystem::path& path);
    static std::vector<Prototype> defaults();

    Prediction classify(const std::vector<double>& features) const;
    const std::vector<Prototype>& prototypes() const noexcept;

private:
    std::vector<Prototype> prototypes_;
};

class PredictionSmoother {
public:
    explicit PredictionSmoother(std::size_t windowSize = 5);

    std::string update(const Prediction& prediction);
    double stability() const;
    void reset();

private:
    std::size_t windowSize_;
    std::deque<std::string> labels_;
};

} // namespace ai_sign
