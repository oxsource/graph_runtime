#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/framework/profiler/reporter/reporter.h"

namespace graph::runtime {
namespace {

struct Args {
  std::vector<std::string> files;
  std::string node_filter;
  bool compare = false;
  std::string format = "table";
  std::string output_path;
};

Args ParseArgs(int argc, char* argv[]) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg.substr(0, 7) == "--files" || arg.substr(0, 7) == "--file=") {
      std::string val = (arg[7] == '=')
          ? arg.substr(8) : (i + 1 < argc ? std::string(argv[++i]) : "");
      size_t pos = 0;
      while ((pos = val.find(',')) != std::string::npos) {
        args.files.push_back(val.substr(0, pos));
        val.erase(0, pos + 1);
      }
      if (!val.empty()) args.files.push_back(val);
    } else if (arg.substr(0, 14) == "--node-filter=") {
      args.node_filter = arg.substr(14);
    } else if (arg == "--compare") {
      args.compare = true;
    } else if (arg.substr(0, 9) == "--format=") {
      args.format = arg.substr(9);
    } else if (arg.substr(0, 9) == "--output=") {
      args.output_path = arg.substr(9);
    }
  }
  return args;
}

bool MatchesFilter(const std::string& name,
                   const std::string& filter) {
  if (filter.empty()) return true;
  if (filter == "*") return true;
  if (filter.find('*') != std::string::npos) {
    std::string prefix = filter.substr(0, filter.find('*'));
    std::string suffix = filter.substr(filter.find('*') + 1);
    if (name.size() >= prefix.size() + suffix.size() &&
        name.substr(0, prefix.size()) == prefix &&
        name.substr(name.size() - suffix.size()) == suffix) {
      return true;
    }
    return false;
  }
  return name == filter;
}

void PrintTable(const ProfileReport& report,
                const std::string& node_filter,
                std::ostream& os) {
  auto print_separator = [&os]() {
    os << "──────────────────────  ─────  "
          "────────  ─────────  ────────  ─────────\n";
  };

  os.width(22); os << "Node";
  os << "  " << "Count";
  os << "  " << "Mean(ms)";
  os << "  " << "Total(ms)";
  os << "  " << "Open(ms)";
  os << "  " << "Close(ms)";
  os << "\n";
  print_separator();

  for (const auto& node : report.nodes) {
    if (!MatchesFilter(node.node_name, node_filter)) continue;
    char buf[64];
    os.width(22); os << node.node_name;
    os << "  ";
    os.width(5); os << node.process_count;
    os << "  ";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  node.process_time_mean_usec / 1000.0);
    os.width(8); os << buf;
    os << "  ";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  static_cast<double>(node.process_time_total_usec) / 1000.0);
    os.width(8); os << buf;
    os << "  ";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  static_cast<double>(node.open_runtime_usec) / 1000.0);
    os.width(8); os << buf;
    os << "  ";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  static_cast<double>(node.close_runtime_usec) / 1000.0);
    os.width(8); os << buf;
    os << "\n";
  }
  print_separator();
  os.width(22); os << "TOTAL";
  os << "  ";
  os.width(5); os << report.total_process_count;
  os << "  ";
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f",
      report.total_process_time_usec > 0
          ? static_cast<double>(report.total_process_time_usec) /
                report.total_process_count / 1000.0
          : 0.0);
  os.width(8); os << buf;
  os << "  ";
  std::snprintf(buf, sizeof(buf), "%.3f",
                static_cast<double>(report.total_process_time_usec) / 1000.0);
  os.width(8); os << buf;
  os << "  ";
  os << "  " << "—";
  os << "       " << "—";
  os << "\n";
}

void PrintCsv(const ProfileReport& report,
              const std::string& node_filter,
              std::ostream& os) {
  os << "Node,Count,Mean(ms),Total(ms),Open(ms),Close(ms)\n";
  for (const auto& node : report.nodes) {
    if (!MatchesFilter(node.node_name, node_filter)) continue;
    char buf[64];
    os << node.node_name << ","
       << node.process_count << ",";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  node.process_time_mean_usec / 1000.0);
    os << buf << ",";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  static_cast<double>(node.process_time_total_usec) / 1000.0);
    os << buf << ",";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  static_cast<double>(node.open_runtime_usec) / 1000.0);
    os << buf << ",";
    std::snprintf(buf, sizeof(buf), "%.3f",
                  static_cast<double>(node.close_runtime_usec) / 1000.0);
    os << buf << "\n";
  }
}

void PrintDeltaTable(const std::vector<Reporter::Delta>& deltas,
                     std::ostream& os) {
  auto print_sep = [&os]() {
    os << "──────────────────────  ────────  ─────────  ─────────\n";
  };

  os.width(22); os << "Node";
  os << "  " << "Mean(ms)";
  os << "  " << "Delta(ms)";
  os << "  " << "Delta(%)";
  os << "\n";
  print_sep();

  char buf[64];
  for (const auto& d : deltas) {
    os.width(22); os << d.node_name;
    os << "  ";
    os.width(8);
    std::snprintf(buf, sizeof(buf), "%.3f", d.process_mean_delta_usec / 1000.0);
    os << buf;
    os << "  ";
    os.width(9);
    std::snprintf(buf, sizeof(buf), "%+.3f", d.process_mean_delta_usec / 1000.0);
    os << buf;
    os << "  ";
    os.width(9);
    std::snprintf(buf, sizeof(buf), "%+.1f%%", d.process_mean_delta_pct);
    os << buf;
    os << "\n";
  }
}

int Run(const Args& args) {
  if (args.files.empty()) {
    std::cerr << "Error: --files is required\n";
    return 1;
  }

  Reporter reporter;
  for (const auto& file : args.files) {
    auto status = reporter.Accumulate(file);
    if (!status.ok()) {
      std::cerr << "Error: " << status.ToString() << "\n";
      return 1;
    }
  }

  std::ostream* os = &std::cout;
  std::ofstream ofs;
  if (!args.output_path.empty()) {
    ofs.open(args.output_path);
    if (!ofs.is_open()) {
      std::cerr << "Error: cannot open output file: "
                << args.output_path << "\n";
      return 1;
    }
    os = &ofs;
  }

  if (args.compare && args.files.size() >= 2) {
    Reporter baseline_reporter;
    auto status = baseline_reporter.Accumulate(args.files[0]);
    if (!status.ok()) {
      std::cerr << "Error: " << status.ToString() << "\n";
      return 1;
    }
    ProfileReport baseline = baseline_reporter.Report();

    Reporter experiment_reporter;
    for (size_t i = 1; i < args.files.size(); ++i) {
      auto st = experiment_reporter.Accumulate(args.files[i]);
      if (!st.ok()) {
        std::cerr << "Error: " << st.ToString() << "\n";
        return 1;
      }
    }
    ProfileReport experiment = experiment_reporter.Report();

    ProfileReport filtered;
    for (const auto& node : experiment.nodes) {
      if (MatchesFilter(node.node_name, args.node_filter)) {
        filtered.nodes.push_back(node);
      }
    }
    filtered.total_process_count = 0;
    filtered.total_process_time_usec = 0;
    for (const auto& node : filtered.nodes) {
      filtered.total_process_count += node.process_count;
      filtered.total_process_time_usec += node.process_time_total_usec;
    }

    auto deltas = experiment_reporter.Compare(baseline);
    if (args.format == "csv") {
      *os << "Node,Mean(ms),Delta(ms),Delta(%)\n";
      for (const auto& d : deltas) {
        if (!MatchesFilter(d.node_name, args.node_filter)) continue;
        char buf[64];
        *os << d.node_name << ",";
        std::snprintf(buf, sizeof(buf), "%.3f",
                      d.process_mean_delta_usec / 1000.0);
        *os << buf << ",";
        std::snprintf(buf, sizeof(buf), "%.3f",
                      d.process_mean_delta_usec / 1000.0);
        *os << buf << ","
            << d.process_mean_delta_pct << "\n";
      }
    } else {
      PrintDeltaTable(deltas, *os);
    }
  } else {
    ProfileReport report = reporter.Report();
    if (args.format == "csv") {
      PrintCsv(report, args.node_filter, *os);
    } else {
      PrintTable(report, args.node_filter, *os);
    }
  }

  return 0;
}

}  // namespace

}  // namespace graph::runtime

int main(int argc, char* argv[]) {
  graph::runtime::Args args =
      graph::runtime::ParseArgs(argc, argv);
  return graph::runtime::Run(args);
}
