# Quickstart: Configuration Architecture — IGraphConfigParser & JSON Parser

## Architecture Overview

```
Pipeline.json ──► JsonParser ──► IGraphConfigParser ──► GraphConfig
                    (nlohmann)        (abstract)          │
                                                          ▼
                                                   GraphBuilder::Build()
                                                          │
                                                          ▼
                                                   GraphRuntime
```

The config parsing layer is fully decoupled from the runtime via `IGraphConfigParser`.

## Writing a JSON Config

```json
{
  "nodes": [
    {
      "name": "producer",
      "type": "StringProducer",
      "output_streams": ["output:text_stream"]
    },
    {
      "name": "transformer",
      "type": "StringUppercase",
      "input_streams": ["input:text_stream"],
      "output_streams": ["output:upper_stream"]
    },
    {
      "name": "consumer",
      "type": "StringConsumer",
      "input_streams": ["input:upper_stream"]
    }
  ],
  "streams": [
    { "name": "text_stream", "source_node": "producer", "source_port": "output",
      "dest_node": "transformer", "dest_port": "input" },
    { "name": "upper_stream", "source_node": "transformer", "source_port": "output",
      "dest_node": "consumer", "dest_port": "input" }
  ]
}
```

## Using the Parser

```cpp
#include "graph_runtime/graph_runtime.h"
#include "graph_runtime/config.h"  // or direct include

// Auto-detect format by extension
auto parser = ParserRegistry::CreateForFile("pipeline.json");
auto config = parser->Parse("pipeline.json");
if (!config.ok()) {
  LOG(ERROR) << config.status();
  return;
}

// Build and run (same as programmatic config)
auto runtime = GraphBuilder::Build(*config);
runtime->Start();
runtime->WaitUntilDone();
```

Or with explicit parser:

```cpp
JsonParser parser;
auto config = parser.Parse("pipeline.json");
auto runtime = GraphBuilder::Build(*config);
```

## Integration with GraphRuntime

`GraphRuntime::Initialize()` will support a file-based overload:

```cpp
GraphRuntime runtime;
runtime.Initialize("pipeline.json");  // auto-detects format
runtime.Start();
runtime.WaitUntilDone();
```

## Adding a New Format (Phase 2)

```cpp
class YamlParser : public IGraphConfigParser {
  absl::StatusOr<GraphConfig> Parse(const std::string& file_path) override;
};

// Register it
GRAPH_RUNTIME_REGISTER_PARSER("yaml", YamlParser);
GRAPH_RUNTIME_REGISTER_PARSER("yml", YamlParser);
```

## Implementation Checklist

- [ ] IGraphConfigParser abstract interface
- [ ] ConfigValidator with all 4 validation rules
- [ ] ParserRegistry with extension-based dispatch
- [ ] JsonParser (uses nlohmann/json)
- [ ] JSON testdata files (valid + invalid cases)
- [ ] config_parser_test.cc (unit tests for all parser + validator)
- [ ] Integration: GraphRuntime::Initialize(file_path) overload
