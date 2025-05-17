#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <string>
#include <unordered_map>
#include <cctype>


void main(size_t argc, char8_t* argv[]) {
  if (argc < 2) {
    std::cout << "Usage:" << std::endl;
    std::cout << std::filesystem::path(argv[0]).stem().string() << " <script_directory> [<indices_csv>]" << std::endl;
    return;
  }

  auto ReadFile = [&] (const std::filesystem::directory_entry& directoryEntry) -> std::vector<char> {
    std::vector<char> data(directoryEntry.file_size());

    std::ifstream inputFile(directoryEntry.path());
    inputFile.read(data.data(), data.size());
    inputFile.close();

    return data;
  };

  // Read in indices CSV file
  std::unordered_map<std::string, uint64_t> indices;
  if (argc > 2) {
    std::vector<char> data{ ReadFile(std::filesystem::directory_entry(argv[2])) };

    char* c = data.data();
    const char* const end = data.data() + data.size();
    // Skip first line "Name,Index"
    while (c < end && *c++ != '\n');

    std::vector<char> currentLine;
    for (; c < end; ++c) {
      if (*c == ',') {
        currentLine.push_back('\0');
        indices.insert({ currentLine.data() , std::stoll(++c) });
        currentLine.clear();
        while (c < end && *++c != '\n');
      } else {
        currentLine.push_back(std::tolower(*c));
      }
    }
  }

  // Output CSV file
  std::cout << "Index,Filename,Tempo,Tick Length,Looping,Loop Start" << std::endl;

  for (const std::filesystem::directory_entry& directoryEntry : std::filesystem::recursive_directory_iterator(argv[1])) {
    if (directoryEntry.is_directory() || directoryEntry.path().extension() != u8".s") {
      continue;
    }

    std::vector<char> data{ ReadFile(directoryEntry) };

    std::vector<char> currentLine;
    int64_t tempo = -1;
    int64_t tickCount = 0;
    int64_t loopStartPoint = -1;
    bool looping = false;

    for (const char& c : data) {
      if (c == '\n') {
        currentLine.push_back('\0');
        std::string line(currentLine.data());

        // Read in tempo (ticks/s = TEMPO / 80 * 64)
        if (line.starts_with("\t.byte\tTEMPO , ")) {
          tempo = std::stoll(line.substr(_countof("\t.byte\tTEMPO , ") - 1).c_str()) / 2;

        } else if (line.starts_with("\t.byte TEMPO, 0x")) {
            tempo = std::stoll(line.substr(_countof("\t.byte TEMPO, 0x") - 1).c_str(), nullptr, 16);

        // Wxx - Wait number of ticks
        } else if (line.starts_with("\t.byte\tW") || line.starts_with("\t.byte W")) {
          tickCount += std::stoll(line.substr(_countof("\t.byte\tW") - 1).c_str());

        // Loop labels are named with _B1 by convention
        } else if (line.ends_with("_B1:")) {
          loopStartPoint = tickCount;

        // Reached end of track
        } else if (line.starts_with("\t.byte\tFINE") || line.starts_with("\t.byte FINE")) {
          break;

        // Check if a GOTO exists for robustness
        } else if (line.starts_with("\t.byte\tGOTO") || line.starts_with("\t.byte GOTO")) {
          looping = true;
          break;
        }

        currentLine.clear();

      } else {
        currentLine.push_back(c);
      }
    }

    int64_t index = -1;

    std::string name{ directoryEntry.path().stem().string() };
    for (char& c : name) {
      c = std::tolower(c);
    }

    auto it = indices.find(name);
    if (it != indices.end()) {
      index = it->second;
    }

    std::cout << index << ',' << name << ',' << tempo << ','
      << tickCount << ',' << (looping ? "TRUE" : "FALSE") << ',' << loopStartPoint << std::endl;

#if _DEBUG
    if (argc > 2 && index == -1) {
      std::cout << "* Error: Index not found." << std::endl;
    }

    if (tickCount == 0) {
      std::cout << "* Error: Length of 0." << std::endl;
    }

    if (tempo == -1) {
      std::cout << "* Error: Tempo not found." << std::endl;
    }

    if (looping && loopStartPoint == -1) {
      std::cout << "* Error: GOTO found, but no label." << std::endl;
    }
#endif

  }
}
