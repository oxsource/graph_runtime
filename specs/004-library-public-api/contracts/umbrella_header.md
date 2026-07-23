# Contract: Umbrella Header — graph_runtime.h

**File**: `graph_runtime/src/public/include/graph_runtime/graph_runtime.h`

```cpp
#if !defined(GRAPH_RUNTIME_GRAPH_RUNTIME_H_)
#define GRAPH_RUNTIME_GRAPH_RUNTIME_H_

#include "graph_runtime/graph_runtime_export.h"
#include "graph_runtime/types.h"
#include "graph_runtime/timestamp.h"
#include "graph_runtime/packet.h"
#include "graph_runtime/graph_config.h"
#include "graph_runtime/side_packet.h"

#endif  // GRAPH_RUNTIME_GRAPH_RUNTIME_H_
```

**Semantics**:
- Single include for all public types.
- External consumers: `#include "graph_runtime/graph_runtime.h"`
- Internal consumers: include specific sub-headers as needed.
- Order: export macro first (required by all other headers), then foundational types, then derived types.
