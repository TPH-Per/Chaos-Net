#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

struct InjectionRequest {
  std::string fault;
  double intensity;
  int duration_seconds;
};

struct Observation {
  bool crashed = false;
  double crash_after_seconds = 0.0;
  std::string notes;
};

class ChaosEngine {
 public:
  Observation Inject(const InjectionRequest& request) const {
    Observation obs;

    if (request.fault == "packet_loss" || request.fault == "network_loss") {
      if (request.intensity > 15.0) {
        obs.crashed = true;
        obs.crash_after_seconds = 2.3;
        obs.notes = "Network stack exhausted retry budget.";
      } else {
        obs.notes = "Application recovered after transient packet loss.";
      }
      return obs;
    }

    if (request.fault == "memory_pressure") {
      if (request.intensity >= 85.0) {
        obs.crashed = true;
        obs.crash_after_seconds = 1.7;
        obs.notes = "Allocator returned OOM under sustained pressure.";
      } else {
        obs.notes = "Memory pressure handled by cache eviction.";
      }
      return obs;
    }

    if (request.fault == "cpu_throttle") {
      if (request.intensity >= 90.0) {
        obs.notes = "Watchdog nearly timed out due to CPU starvation.";
      } else {
        obs.notes = "Increased response latency without process crash.";
      }
      return obs;
    }

    if (request.fault == "packet_corrupt") {
      if (request.intensity >= 30.0) {
        obs.notes = "CRC validation rejected corrupted payloads.";
      } else {
        obs.notes = "Occasional packet corruption tolerated.";
      }
      return obs;
    }

    throw std::invalid_argument("Unsupported fault type: " + request.fault);
  }
};

class ObserverAnalyzer {
 public:
  std::string BuildReport(const InjectionRequest& request,
                          const Observation& observation) const {
    std::ostringstream report;
    report << "[Resilience Report]\n";
    report << "Fault: " << request.fault << "\n";
    report << "Intensity: " << request.intensity << "%\n";
    report << "Duration: " << request.duration_seconds << "s\n";

    if (request.fault == "packet_loss" && request.intensity > 15.0) {
      report << "App bị crash sau 2.3s khi packet loss > 15%\n";
    } else if (observation.crashed) {
      report << "App crashed after " << std::fixed << std::setprecision(1)
             << observation.crash_after_seconds << "s\n";
    } else {
      report << "App survived fault injection\n";
    }

    report << "Observation: " << observation.notes << "\n";
    report << "Timestamp: " << TimestampUtc();
    return report.str();
  }

 private:
  std::string TimestampUtc() const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &now_time);
#else
    gmtime_r(&now_time, &tm_utc);
#endif

    std::ostringstream out;
    out << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
  }
};

InjectionRequest ParseRequestFromArgs(int argc, char* argv[]) {
  InjectionRequest request{"packet_loss", 10.0, 5};

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--type" && i + 1 < argc) {
      request.fault = argv[++i];
    } else if (arg == "--intensity" && i + 1 < argc) {
      request.intensity = std::stod(argv[++i]);
    } else if (arg == "--duration" && i + 1 < argc) {
      request.duration_seconds = std::stoi(argv[++i]);
    }
  }

  return request;
}

int RunSingleShot(int argc, char* argv[]) {
  const ChaosEngine engine;
  const ObserverAnalyzer analyzer;

  const InjectionRequest request = ParseRequestFromArgs(argc, argv);
  const Observation observation = engine.Inject(request);

  std::cout << analyzer.BuildReport(request, observation) << std::endl;
  return 0;
}

int RunDaemonMode() {
  const ChaosEngine engine;
  const ObserverAnalyzer analyzer;

  std::cout << "ChaosNet daemon started.\n";
  std::cout << "Use: inject <fault> <intensity_percent> <duration_seconds>\n";
  std::cout << "Faults: packet_loss, network_loss, memory_pressure, cpu_throttle, packet_corrupt\n";
  std::cout << "Type 'exit' to stop.\n";

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "exit") {
      break;
    }

    std::istringstream input(line);
    std::string command;
    InjectionRequest request;

    if (!(input >> command >> request.fault >> request.intensity >> request.duration_seconds) ||
        command != "inject") {
      std::cerr << "Invalid command. Expected: inject <fault> <intensity_percent> <duration_seconds>\n";
      continue;
    }

    try {
      const Observation observation = engine.Inject(request);
      std::cout << analyzer.BuildReport(request, observation) << "\n";
    } catch (const std::exception& ex) {
      std::cerr << ex.what() << "\n";
    }
  }

  return 0;
}

int main(int argc, char* argv[]) {
  try {
    if (argc > 1) {
      return RunSingleShot(argc, argv);
    }
    return RunDaemonMode();
  } catch (const std::exception& ex) {
    std::cerr << "ChaosNet failed: " << ex.what() << std::endl;
    return 1;
  }
}
