#!/bin/bash
# Integration test for MPI distributed mode
# Requires: mpirun (OpenMPI), cmake -DDRC_USE_MPI=ON build in build_mpi/
# Usage: ./run_mpi_integration.sh [build_dir]
set -e

BUILD_DIR="${1:-build_mpi}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DRC_CHECK="$PROJECT_DIR/$BUILD_DIR/src/cli/drc-check"
LAYOUT="$PROJECT_DIR/testdata/layouts/basic_input.gds"

if [ ! -f "$DRC_CHECK" ]; then
    echo "SKIP: MPI build not found at $DRC_CHECK"
    exit 0
fi

if ! command -v mpirun &> /dev/null; then
    echo "SKIP: mpirun not available"
    exit 0
fi

TOTAL=0
PASSED=0

# Test 1: Basic MPI run with 3 processes
echo "Test 1: MPI basic run (3 processes)"
cat > /tmp/test_mpi_basic.drc << 'EOF'
source("testdata/layouts/basic_input.gds")
target("/tmp/test_mpi_basic_out.gds")
m1 = input(10, 0)
m1:output(1, 0)
write()
EOF

if mpirun -np 3 --allow-run-as-root "$DRC_CHECK" "/tmp/test_mpi_basic.drc" 2>&1; then
    echo "  PASS"
    PASSED=$((PASSED+1))
else
    echo "  FAIL"
fi
TOTAL=$((TOTAL+1))

# Test 2: MPI with boolean ops
echo "Test 2: MPI boolean operations"
cat > /tmp/test_mpi_boolean.drc << 'EOF'
source("testdata/layouts/boolean_ops.gds")
target("/tmp/test_mpi_boolean_out.gds")
a = input(10, 0)
b = input(20, 0)
c = a & b
d = a | b
c:output(1, 0)
d:output(2, 0)
write()
EOF

if mpirun -np 3 --allow-run-as-root "$DRC_CHECK" "/tmp/test_mpi_boolean.drc" 2>&1; then
    echo "  PASS"
    PASSED=$((PASSED+1))
else
    echo "  FAIL"
fi
TOTAL=$((TOTAL+1))

# Test 3: MPI with DRC checks
echo "Test 3: MPI DRC checks"
cat > /tmp/test_mpi_checks.drc << 'EOF'
source("testdata/layouts/width_space_check.gds")
target("/tmp/test_mpi_checks_out.gds")
m1 = input(10, 0)
w = m1:width(0.2)
s = m1:space(0.2)
w:output(1, 0)
s:output(2, 0)
write()
EOF

if mpirun -np 3 --allow-run-as-root "$DRC_CHECK" "/tmp/test_mpi_checks.drc" 2>&1; then
    echo "  PASS"
    PASSED=$((PASSED+1))
else
    echo "  FAIL"
fi
TOTAL=$((TOTAL+1))

# Test 4: Error - need at least 2 processes
echo "Test 4: Error with single process"

if mpirun -np 1 --allow-run-as-root "$DRC_CHECK" "/tmp/test_mpi_basic.drc" 2>&1; then
    echo "  FAIL (expected error)"
else
    echo "  PASS (correctly rejected)"
    PASSED=$((PASSED+1))
fi
TOTAL=$((TOTAL+1))

echo ""
echo "Results: $PASSED/$TOTAL passed"

if [ "$PASSED" -eq "$TOTAL" ]; then
    exit 0
else
    exit 1
fi
