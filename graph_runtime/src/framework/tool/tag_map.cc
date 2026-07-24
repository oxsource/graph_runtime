#include "src/framework/tool/tag_map.h"

#include "absl/status/status.h"
#include "src/framework/tool/validate_name.h"

namespace graph::runtime {

absl::StatusOr<TagMap> TagMap::Create(
    const std::vector<std::string>& tag_index_names) {
  TagMap tag_map;
  int id = 0;
  // Build a map from tag to list of (index, name) pairs, preserving order.
  // Use a vector for order tracking and a map for fast lookup.
  struct EntryGroup {
    std::string tag;
    std::vector<std::pair<int, std::string>> entries;
  };
  std::vector<EntryGroup> groups;

  for (const auto& entry : tag_index_names) {
    auto parsed = ParseTagIndexName(entry);
    if (!parsed.ok()) return parsed.status();
    // Find or create the group for this tag.
    bool found = false;
    for (auto& g : groups) {
      if (g.tag == parsed->tag) {
        g.entries.push_back({parsed->index, parsed->name});
        found = true;
        break;
      }
    }
    if (!found) {
      groups.push_back({parsed->tag, {{parsed->index, parsed->name}}});
    }
  }

  // Build mapping and names in insertion order.
  for (auto& group : groups) {
    std::sort(group.entries.begin(), group.entries.end());
    TagMap::TagData data;
    data.first_id = tag_map.num_entries_;
    data.count = static_cast<int>(group.entries.size());
    tag_map.mapping_[group.tag] = data;
    tag_map.tag_order_.push_back(group.tag);
    for (const auto& [idx, name] : group.entries) {
      (void)idx;
      tag_map.names_.push_back(name);
    }
    tag_map.num_entries_ += group.entries.size();
  }

  return tag_map;
}

int TagMap::NumEntries(const std::string& tag) const {
  auto it = mapping_.find(tag);
  if (it == mapping_.end()) return 0;
  return it->second.count;
}

bool TagMap::HasTag(const std::string& tag) const {
  return mapping_.find(tag) != mapping_.end();
}

std::set<std::string> TagMap::GetTags() const {
  std::set<std::string> tags;
  for (const auto& [tag, _] : mapping_) {
    tags.insert(tag);
  }
  return tags;
}

CollectionItemId TagMap::GetId(const std::string& tag, int index) const {
  auto it = mapping_.find(tag);
  if (it == mapping_.end()) return -1;
  if (index < 0 || index >= it->second.count) return -1;
  return it->second.first_id + index;
}

CollectionItemId TagMap::GetId(const std::string& tag_index_name) const {
  auto parsed = ParseTagIndexName(tag_index_name);
  if (!parsed.ok()) return -1;
  if (parsed->tag.empty()) return -1;  // plain name, not valid here
  return GetId(parsed->tag, parsed->index);
}

}  // namespace graph::runtime
