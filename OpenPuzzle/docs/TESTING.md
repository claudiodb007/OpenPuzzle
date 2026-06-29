# Testing

Current smoke test:

```bash
./scripts/smoke_test.sh
```

The smoke test verifies:

- build
- database initialization
- Puzzle 71 import
- job creation
- range listing
- dashboard command

Future tests should cover:

- RangeAllocator overlap prevention
- external range avoidance
- BitCrack command generation
- GPU selection
- execution lifecycle
