# MasteringService

An automatic audio mastering service, run from the command line. Give it an audio file and it writes a mastered version of it.

Built with [JUCE](https://juce.com) **9.0.3**, which is downloaded automatically by CMake (via `FetchContent`) the first time you configure the project. You don't need to install JUCE yourself.

## Requirements

- Linux
- A C++20 compiler (GCC or Clang)
- [CMake](https://cmake.org) 4.2 or newer
- [Ninja](https://ninja-build.org)
- Git (used by CMake to fetch JUCE)

### Dependencies

On Debian/Ubuntu-based distributions, install the required system libraries with:

```bash
sudo apt update
sudo apt install libasound2-dev libx11-dev libxcomposite-dev libxext-dev
```

## Building

From the root of the repository:

```bash
# Configure (this fetches JUCE 9.0.3 the first time)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build
```

The executable is generated at:

```
build/MasteringService_artefacts/Release/MasteringService
```

To build a debug version instead, use `-DCMAKE_BUILD_TYPE=Debug`; the executable will then be in the `Debug` folder.

## Usage

```
MasteringService [options]
```

### Options

| Option              | Description                                  |
|---------------------|----------------------------------------------|
| `--file`, `-f`      | Master an audio file (see below)             |
| `--help`, `-h`      | Show usage information                       |
| `--version`, `-v`   | Show the application version                 |

### Mastering a file

```bash
./MasteringService -f <input_file> <output_file>
```

- `<input_file>`: the audio file you want to master. It must exist.
- `<output_file>`: where the mastered audio will be written.

Example:

```bash
./MasteringService -f my_song.wav my_song_mastered.wav
```

When the process finishes successfully, the full path of the mastered file is printed to standard output, which makes it easy to use from scripts or other services:

```bash
MASTERED=$(./MasteringService -f my_song.wav my_song_mastered.wav)
echo "Mastered file: $MASTERED"
```

## Version

Current version: `1.0.0`

## Credits

Powered by [JUCE](https://github.com/juce-framework/JUCE) 9.0.3.
