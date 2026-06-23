# Live AI Sign Language Classification

A C++17 console application that classifies live-style hand landmark frames into common sign language gestures.

The app is dependency-free by default. It replays a sample landmark feed in real time, applies a nearest-prototype AI classifier, smooths predictions across frames, and builds a session transcript. The frame-source layer is separate from the classifier so a webcam, MediaPipe, OpenCV, or ONNX Runtime pipeline can be added later without rewriting the app flow.

## Project layout

```text
LiveAISignLanguageClassification/
  CMakeLists.txt
  include/ai_sign/
  src/
  data/sign_prototypes.csv
  data/sample_frames.csv
  scripts/build-mingw.bat
```

## Build

Using CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Using a MinGW-style compiler directly:

```powershell
scripts\build-mingw.bat
```

If `g++` is not on `PATH`, pass the compiler path:

```powershell
scripts\build-mingw.bat C:\path\to\g++.exe
```

## Run

```powershell
.\build\Release\LiveAISignLanguageClassification.exe
```

For single-config CMake generators:

```powershell
.\build\LiveAISignLanguageClassification.exe
```

Direct MinGW build output:

```powershell
.\LiveAISignLanguageClassification.exe
```

## Options

```text
--prototypes <path>   Prototype landmark CSV file.
--input <path>        Live/replayed landmark frame CSV file.
--synthetic           Ignore input CSV and generate a synthetic live stream.
--iterations <n>      Number of frames to process. Default: 100.
--speed-ms <n>        Delay between frames. Default: 160.
--no-clear            Print frames continuously instead of refreshing the console.
--help                Show usage.
```

## Data format

`data/sign_prototypes.csv`

```text
label,display,f0,f1,...,f11
HELLO,Hello,0.10,0.88,...
```

`data/sample_frames.csv`

```text
frame_id,source_label,f0,f1,...,f11
demo-001,HELLO,0.11,0.87,...
```

The classifier expects normalized numeric hand-landmark features. A real camera pipeline should output the same feature vector shape per frame.
