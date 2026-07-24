#include "src/config/parser_registry.h"

#include <map>
#include <mutex>

namespace graph::runtime {

namespace {

struct RegistryData {
  std::map<std::string, ParserRegistry::ParserFactory> factories;
  std::mutex mutex;
};

RegistryData& Data() {
  static RegistryData data;
  return data;
}

std::string ExtensionFromPath(const std::string& path) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos) return "";
  auto ext = path.substr(dot + 1);
  for (auto& c : ext) c = static_cast<char>(std::tolower(c));
  return ext;
}

}  // namespace

void ParserRegistry::Register(const std::string& extension,
                               ParserFactory factory) {
  auto& data = Data();
  std::lock_guard<std::mutex> lock(data.mutex);
  data.factories[extension] = std::move(factory);
}

std::unique_ptr<IGraphConfigParser> ParserRegistry::CreateForFile(
    const std::string& file_path) {
  auto ext = ExtensionFromPath(file_path);
  auto& data = Data();
  std::lock_guard<std::mutex> lock(data.mutex);
  auto it = data.factories.find(ext);
  if (it == data.factories.end()) return nullptr;
  return it->second();
}

bool ParserRegistry::IsFormatSupported(const std::string& file_path) {
  auto ext = ExtensionFromPath(file_path);
  auto& data = Data();
  std::lock_guard<std::mutex> lock(data.mutex);
  return data.factories.find(ext) != data.factories.end();
}

}  // namespace graph::runtime
