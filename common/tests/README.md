# Orderbook Integration Tests

This directory contains integration tests for the orderbook core functionality.

## Test Cases

### Order Matching Tests

Tests basic order matching functionality in the orderbook:

1. **Basic Matching**: Tests simple buy/sell order matching
2. **Price Priority**: Verifies orders are matched by price priority (best prices first)
3. **Time Priority**: Verifies orders at the same price are matched by time priority (FIFO)
4. **Tick Size Rounding**: Tests the tick size feature and price rounding/validation

## Running the Tests

To run these tests, you need to configure CMake with the `BUILD_TESTS` option:

```bash
mkdir -p build
cd build
cmake -DBUILD_TESTS=ON ..
cmake --build . --target orderbook_matching_test
./bin/orderbook_matching_test
```

## GitHub Actions Integration

These tests are automatically run in GitHub Actions CI when changes are pushed to the `common` directory.

The workflow is defined in `.github/workflows/orderbook_tests.yml` and includes:

1. Integration tests that verify order matching functionality
2. Memory safety tests using Valgrind
