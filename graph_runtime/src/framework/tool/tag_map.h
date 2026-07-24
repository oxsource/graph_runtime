#ifndef GRAPH_RUNTIME_TAG_MAP_H_
#define GRAPH_RUNTIME_TAG_MAP_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "src/framework/public/types.h"

namespace graph::runtime {

// TagMap maps tag strings to index ranges, following MediaPipe's
// tag/index scheme for streams and side packets.
//
// Given input streams {"VIDEO:0:left", "VIDEO:1:right", "AUDIO:0"},
// TagMap creates:
//   "VIDEO" → {first_id=0, count=2}  (VIDEO:0, VIDEO:1)
//   "AUDIO" → {first_id=2, count=1}  (AUDIO:0)
//
// Names: ["left", "right", ""]
class TagMap {
 public:
  struct TagData {
    CollectionItemId first_id;
    int count;
  };

  // Create a TagMap from a list of "TAG:index:name" strings.
  static absl::StatusOr<TagMap> Create(
      const std::vector<std::string>& tag_index_names);

  // Returns the mapping from tag to tag data.
  const std::map<std::string, TagData>& Mapping() const { return mapping_; }

  // Returns the vector of names (indexed by CollectionItemId).
  const std::vector<std::string>& Names() const { return names_; }

  // Total number of entries across all tags.
  int NumEntries() const { return num_entries_; }

  // Number of entries under a specific tag.
  int NumEntries(const std::string& tag) const;

  // Returns true if the tag exists.
  bool HasTag(const std::string& tag) const;

  // Returns the set of all tags.
  std::set<std::string> GetTags() const;

  // Get the CollectionItemId for a tag and index.
  // Returns -1 if tag or index is out of range.
  CollectionItemId GetId(const std::string& tag, int index) const;

  // Resolve a "TAG:index" or "TAG:index:name" string to an item id.
  // Returns -1 on parse error or invalid tag/index.
  CollectionItemId GetId(const std::string& tag_index_name) const;

 private:
  TagMap() = default;

  int num_entries_ = 0;
  std::map<std::string, TagData> mapping_;
  std::vector<std::string> names_;
  std::vector<std::string> tag_order_;  // insertion order of tags
};

}  // namespace graph::runtime

#endif  // GRAPH_RUNTIME_TAG_MAP_H_
