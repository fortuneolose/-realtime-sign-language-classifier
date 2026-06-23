#include "ai_sign/Classifier.hpp"
#include "ai_sign/FrameSource.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

struct Options {
    std::filesystem::path prototypesPath = "data/sign_prototypes.csv";
    std::filesystem::path inputPath = "data/sample_frames.csv";
    int iterations = 100;
    int speedMs = 160;
    bool noClear = false;
    bool synthetic = false;
};

void setConsoleTitle() {
#ifdef _WIN32
    SetConsoleTitleA("Live AI Sign Language Classification");
#endif
    std::cout << "\x1B]0;Live AI Sign Language Classification\x07";
}

void clearScreen() {
    std::cout << "\x1B[2J\x1B[H";
}

void printUsage(const char* executable) {
    std::cout
        << "Live AI Sign Language Classification\n\n"
        << "Usage:\n"
        << "  " << executable << " [options]\n\n"
        << "Options:\n"
        << "  --prototypes <path>   Prototype landmark CSV file.\n"
        << "  --input <path>        Live/replayed landmark frame CSV file.\n"
        << "  --synthetic           Ignore input CSV and generate a synthetic live stream.\n"
        << "  --iterations <n>      Number of frames to process. Default: 100.\n"
        << "  --speed-ms <n>        Delay between frames. Default: 160.\n"
        << "  --no-clear            Print frames continuously instead of refreshing the console.\n"
        << "  --help                Show this help text.\n";
}

int parsePositiveInt(const std::string& value, const std::string& name) {
    try {
        const int parsed = std::stoi(value);
        if (parsed <= 0) {
            throw std::invalid_argument("not positive");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error(name + " must be a positive integer.");
    }
}

Options parseOptions(int argc, char** argv) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto needValue = [&](const std::string& optionName) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(optionName + " requires a value.");
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--prototypes") {
            options.prototypesPath = needValue(arg);
        } else if (arg == "--input") {
            options.inputPath = needValue(arg);
        } else if (arg == "--synthetic") {
            options.synthetic = true;
        } else if (arg == "--iterations") {
            options.iterations = parsePositiveInt(needValue(arg), arg);
        } else if (arg == "--speed-ms") {
            options.speedMs = parsePositiveInt(needValue(arg), arg);
        } else if (arg == "--no-clear") {
            options.noClear = true;
        } else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    return options;
}

std::filesystem::path executableDir(char* argv0) {
    std::error_code error;
    auto absolute = std::filesystem::absolute(argv0, error);
    if (error) {
        return std::filesystem::current_path();
    }
    return absolute.parent_path();
}

std::filesystem::path resolvePath(const std::filesystem::path& requested, char* argv0) {
    if (requested.empty() || requested.is_absolute()) {
        return requested;
    }

    const auto exeDir = executableDir(argv0);
    const std::vector<std::filesystem::path> bases = {
        std::filesystem::current_path(),
        exeDir,
        exeDir.parent_path(),
        exeDir.parent_path().parent_path(),
    };

    for (const auto& base : bases) {
        std::error_code error;
        const auto candidate = base / requested;
        if (std::filesystem::exists(candidate, error)) {
            return candidate;
        }
    }

    return requested;
}

std::string bar(double value, int width = 26) {
    value = std::max(0.0, std::min(1.0, value));
    const int filled = static_cast<int>(value * static_cast<double>(width) + 0.5);
    return "[" + std::string(filled, '#') + std::string(width - filled, '.') + "]";
}

std::string percent(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << (value * 100.0) << "%";
    return stream.str();
}

std::string joinTranscript(const std::vector<std::string>& transcript) {
    if (transcript.empty()) {
        return "-";
    }

    std::ostringstream stream;
    for (std::size_t i = 0; i < transcript.size(); ++i) {
        if (i > 0) {
            stream << " | ";
        }
        stream << transcript[i];
    }
    return stream.str();
}

std::string displayFor(
    const std::unordered_map<std::string, std::string>& displayNames,
    const std::string& label) {
    if (label == "UNKNOWN") {
        return "Unknown";
    }
    const auto found = displayNames.find(label);
    return found == displayNames.end() ? label : found->second;
}

void render(
    const ai_sign::LandmarkFrame& frame,
    const ai_sign::Prediction& prediction,
    const std::string& stableLabel,
    double stability,
    const std::vector<std::string>& transcript,
    const std::unordered_map<std::string, std::string>& displayNames,
    const std::string& sourceName,
    int frameNumber,
    bool clear) {
    if (clear) {
        clearScreen();
    }

    std::cout
        << "Live AI Sign Language Classification\n"
        << "====================================\n"
        << "Source: " << sourceName << "\n"
        << "Frame:  " << frameNumber << " (" << frame.id << ")\n"
        << "Input:  " << frame.features.size() << " normalized hand-landmark features\n\n"
        << "Current prediction: " << std::left << std::setw(14) << prediction.display
        << "  confidence " << percent(prediction.confidence) << "\n"
        << "Smoothed output:    " << std::left << std::setw(14) << displayFor(displayNames, stableLabel)
        << "  stability  " << percent(stability) << "\n\n"
        << "Top candidates\n";

    for (const auto& candidate : prediction.top) {
        std::cout
            << "  " << std::left << std::setw(14) << candidate.display
            << " " << bar(candidate.confidence)
            << " " << std::right << std::setw(6) << percent(candidate.confidence)
            << "  distance " << std::fixed << std::setprecision(3) << candidate.distance << "\n";
    }

    std::cout
        << "\nTranscript: " << joinTranscript(transcript) << "\n"
        << "Press Ctrl+C to stop.\n"
        << std::flush;

    if (!clear) {
        std::cout << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        setConsoleTitle();

        const auto prototypePath = resolvePath(options.prototypesPath, argv[0]);
        std::vector<ai_sign::Prototype> prototypes;

        if (std::filesystem::exists(prototypePath)) {
            prototypes = ai_sign::NearestPrototypeClassifier::loadFromCsv(prototypePath);
        } else {
            prototypes = ai_sign::NearestPrototypeClassifier::defaults();
            std::cerr << "Prototype file not found; using built-in defaults.\n";
        }

        ai_sign::NearestPrototypeClassifier classifier(prototypes);
        ai_sign::PredictionSmoother smoother(5);

        std::unordered_map<std::string, std::string> displayNames;
        for (const auto& prototype : classifier.prototypes()) {
            displayNames[prototype.label] = prototype.display;
        }

        std::unique_ptr<ai_sign::IFrameSource> source;
        const auto inputPath = resolvePath(options.inputPath, argv[0]);

        if (!options.synthetic && std::filesystem::exists(inputPath)) {
            source = std::make_unique<ai_sign::CsvFrameSource>(
                ai_sign::CsvFrameSource::loadFromCsv(inputPath),
                true);
        } else {
            source = std::make_unique<ai_sign::SyntheticFrameSource>(classifier.prototypes());
        }

        std::vector<std::string> transcript;
        std::string lastTranscriptLabel;

        for (int frameNumber = 1; frameNumber <= options.iterations; ++frameNumber) {
            auto frame = source->next();
            if (!frame) {
                break;
            }

            auto prediction = classifier.classify(frame->features);
            const std::string stableLabel = smoother.update(prediction);
            const double stability = smoother.stability();

            if (stableLabel != "UNKNOWN" && stableLabel != lastTranscriptLabel && stability >= 0.55) {
                transcript.push_back(displayFor(displayNames, stableLabel));
                lastTranscriptLabel = stableLabel;
            }

            render(
                *frame,
                prediction,
                stableLabel,
                stability,
                transcript,
                displayNames,
                source->name(),
                frameNumber,
                !options.noClear);

            std::this_thread::sleep_for(std::chrono::milliseconds(options.speedMs));
        }

        std::cout << "\nSession complete. Transcript: " << joinTranscript(transcript) << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n\n";
        printUsage(argv[0]);
        return 1;
    }
}
