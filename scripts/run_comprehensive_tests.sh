#!/bin/bash

# SS P2P Node Controller - Comprehensive Test Suite Runner
# This script builds and runs all comprehensive tests for the SS P2P Node Controller

set -e  # Exit on any error

echo "======================================"
echo "SS P2P Node Controller Test Suite"
echo "======================================"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_BUILD_DIR="$PROJECT_ROOT/build_tests"

echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Test build directory: $TEST_BUILD_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check for required tools
check_dependencies() {
    print_status "Checking dependencies..."
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is required but not installed"
        exit 1
    fi
    
    # Check for C++ compiler
    if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
        print_error "C++ compiler (g++ or clang++) is required but not installed"
        exit 1
    fi
    
    print_success "All dependencies found"
}

# Build main library
build_main_library() {
    print_status "Building main SS P2P library..."
    
    cd "$PROJECT_ROOT"
    
    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    cmake .. -DCMAKE_BUILD_TYPE=Debug \
             -DENABLE_TESTING=ON \
             -DENABLE_COVERAGE=OFF
    
    # Build the library
    make -j$(nproc 2>/dev/null || echo 4) ss_p2p
    
    print_success "Main library built successfully"
}

# Build individual test executables
build_test_executable() {
    local test_name="$1"
    local test_source="$2"
    local include_dirs="$3"
    local link_libs="$4"
    
    print_status "Building test: $test_name"
    
    cd "$TEST_BUILD_DIR"
    
    # Compile the test
    g++ -std=c++20 \
        -Wall -Wextra -g -O0 \
        $include_dirs \
        "$test_source" \
        -o "$test_name" \
        $link_libs \
        -pthread \
        -lgtest -lgtest_main -lgmock
    
    if [ $? -eq 0 ]; then
        print_success "Built test: $test_name"
        return 0
    else
        print_error "Failed to build test: $test_name"
        return 1
    fi
}

# Build all comprehensive tests
build_comprehensive_tests() {
    print_status "Building comprehensive test suite..."
    
    # Create test build directory
    mkdir -p "$TEST_BUILD_DIR"
    
    # Common include directories
    INCLUDE_DIRS="-I$PROJECT_ROOT/include -I$PROJECT_ROOT/src"
    
    # Common link libraries
    LINK_LIBS="-L$BUILD_DIR -lss_p2p -lboost_system -lssl -lcrypto"
    
    # Test files to build
    declare -a test_files=(
        "test_result_comprehensive:$PROJECT_ROOT/tests/unit/core/test_result_comprehensive.cpp"
        "test_udp_transport_comprehensive:$PROJECT_ROOT/tests/unit/network/test_udp_transport_comprehensive.cpp"
        "test_kademlia_routing_comprehensive:$PROJECT_ROOT/tests/unit/dht/test_kademlia_routing_comprehensive.cpp"
        "test_nat_traversal_comprehensive:$PROJECT_ROOT/tests/unit/ice/test_nat_traversal_comprehensive.cpp"
        "test_node_communication_integration:$PROJECT_ROOT/tests/integration/test_node_communication_integration.cpp"
    )
    
    local build_count=0
    local total_tests=${#test_files[@]}
    
    for test_entry in "${test_files[@]}"; do
        IFS=':' read -r test_name test_source <<< "$test_entry"
        
        if [ -f "$test_source" ]; then
            if build_test_executable "$test_name" "$test_source" "$INCLUDE_DIRS" "$LINK_LIBS"; then
                ((build_count++))
            fi
        else
            print_warning "Test source not found: $test_source"
        fi
    done
    
    print_status "Built $build_count/$total_tests comprehensive tests"
    
    if [ $build_count -gt 0 ]; then
        print_success "Comprehensive test suite built successfully"
        return 0
    else
        print_error "Failed to build any comprehensive tests"
        return 1
    fi
}

# Run individual test
run_test() {
    local test_name="$1"
    local test_executable="$TEST_BUILD_DIR/$test_name"
    
    if [ ! -f "$test_executable" ]; then
        print_warning "Test executable not found: $test_executable"
        return 1
    fi
    
    print_status "Running test: $test_name"
    
    # Run test with timeout
    timeout 60s "$test_executable" --gtest_output=xml:"$TEST_BUILD_DIR/${test_name}_results.xml"
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        print_success "Test passed: $test_name"
        return 0
    elif [ $exit_code -eq 124 ]; then
        print_error "Test timed out: $test_name"
        return 1
    else
        print_error "Test failed: $test_name (exit code: $exit_code)"
        return 1
    fi
}

# Run all tests
run_all_tests() {
    print_status "Running comprehensive test suite..."
    
    cd "$TEST_BUILD_DIR"
    
    # List of tests to run
    declare -a tests=(
        "test_result_comprehensive"
        "test_udp_transport_comprehensive"
        "test_kademlia_routing_comprehensive"
        "test_nat_traversal_comprehensive"
        "test_node_communication_integration"
    )
    
    local passed_count=0
    local failed_count=0
    local total_tests=${#tests[@]}
    
    echo ""
    echo "======================================"
    echo "Running Tests"
    echo "======================================"
    
    for test_name in "${tests[@]}"; do
        echo ""
        if run_test "$test_name"; then
            ((passed_count++))
        else
            ((failed_count++))
        fi
    done
    
    echo ""
    echo "======================================"
    echo "Test Results Summary"
    echo "======================================"
    echo "Total tests: $total_tests"
    print_success "Passed: $passed_count"
    if [ $failed_count -gt 0 ]; then
        print_error "Failed: $failed_count"
    else
        echo "Failed: $failed_count"
    fi
    
    # Generate summary report
    generate_test_report
    
    if [ $failed_count -eq 0 ]; then
        print_success "All tests passed!"
        return 0
    else
        print_error "Some tests failed"
        return 1
    fi
}

# Generate test report
generate_test_report() {
    local report_file="$TEST_BUILD_DIR/test_report.html"
    
    print_status "Generating test report: $report_file"
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>SS P2P Node Controller - Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .header { background-color: #f0f0f0; padding: 20px; border-radius: 5px; }
        .summary { margin: 20px 0; }
        .passed { color: green; font-weight: bold; }
        .failed { color: red; font-weight: bold; }
        .test-section { margin: 20px 0; border: 1px solid #ddd; padding: 15px; border-radius: 5px; }
        pre { background-color: #f5f5f5; padding: 10px; border-radius: 3px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="header">
        <h1>SS P2P Node Controller - Comprehensive Test Report</h1>
        <p>Generated on: $(date)</p>
    </div>
    
    <div class="summary">
        <h2>Test Coverage Summary</h2>
        <ul>
            <li><strong>Core Module Tests:</strong> Result handling, type safety, error management</li>
            <li><strong>Network Module Tests:</strong> UDP transport, message handling, concurrent operations</li>
            <li><strong>DHT Module Tests:</strong> Kademlia routing, bucket management, node discovery</li>
            <li><strong>ICE Module Tests:</strong> NAT traversal, STUN/TURN operations, connectivity checks</li>
            <li><strong>Integration Tests:</strong> End-to-end node communication, network resilience</li>
        </ul>
    </div>
    
    <div class="test-section">
        <h2>Generated Test Files</h2>
        <ul>
            <li><code>test_result_comprehensive.cpp</code> - Comprehensive result type testing</li>
            <li><code>test_udp_transport_comprehensive.cpp</code> - UDP transport implementation tests</li>
            <li><code>test_kademlia_routing_comprehensive.cpp</code> - DHT routing and bucket management</li>
            <li><code>test_nat_traversal_comprehensive.cpp</code> - ICE NAT traversal functionality</li>
            <li><code>test_node_communication_integration.cpp</code> - Full node integration scenarios</li>
        </ul>
    </div>
    
    <div class="test-section">
        <h2>Test Categories Covered</h2>
        <h3>Unit Tests</h3>
        <ul>
            <li>Type safety and error handling</li>
            <li>Network transport layer functionality</li>
            <li>DHT operations and routing logic</li>
            <li>ICE candidate gathering and connectivity</li>
        </ul>
        
        <h3>Integration Tests</h3>
        <ul>
            <li>Multi-node network formation</li>
            <li>Point-to-point communication</li>
            <li>Broadcast and multicast messaging</li>
            <li>Network resilience and recovery</li>
            <li>Performance and throughput testing</li>
        </ul>
        
        <h3>Edge Cases and Error Conditions</h3>
        <ul>
            <li>Network failures and timeouts</li>
            <li>Concurrent access and thread safety</li>
            <li>Resource exhaustion scenarios</li>
            <li>Invalid input handling</li>
        </ul>
    </div>
    
    <div class="test-section">
        <h2>XML Test Results</h2>
        <p>Individual test results are available in XML format:</p>
        <ul>
EOF

    # Add XML result files to report if they exist
    for xml_file in "$TEST_BUILD_DIR"/*_results.xml; do
        if [ -f "$xml_file" ]; then
            echo "            <li><a href=\"$(basename "$xml_file")\">$(basename "$xml_file")</a></li>" >> "$report_file"
        fi
    done

    cat >> "$report_file" << EOF
        </ul>
    </div>
    
    <div class="test-section">
        <h2>Next Steps</h2>
        <p>This comprehensive test suite provides:</p>
        <ul>
            <li><strong>Improved Code Coverage:</strong> Tests previously untested code paths</li>
            <li><strong>Better Error Detection:</strong> Identifies edge cases and error conditions</li>
            <li><strong>Performance Validation:</strong> Ensures acceptable performance characteristics</li>
            <li><strong>Integration Verification:</strong> Validates end-to-end functionality</li>
        </ul>
    </div>
</body>
</html>
EOF
    
    print_success "Test report generated: $report_file"
}

# Clean up build artifacts
cleanup() {
    if [ "$1" = "--clean" ]; then
        print_status "Cleaning up build artifacts..."
        rm -rf "$TEST_BUILD_DIR"
        print_success "Cleanup completed"
    fi
}

# Main execution
main() {
    echo "Starting comprehensive test suite for SS P2P Node Controller"
    echo ""
    
    # Parse command line arguments
    if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
        echo "Usage: $0 [--clean] [--build-only] [--run-only]"
        echo ""
        echo "Options:"
        echo "  --clean      Clean build artifacts before building"
        echo "  --build-only Build tests but don't run them"
        echo "  --run-only   Run tests without building (assumes already built)"
        echo "  --help, -h   Show this help message"
        exit 0
    fi
    
    if [ "$1" = "--clean" ]; then
        cleanup --clean
        shift
    fi
    
    # Check dependencies
    check_dependencies
    
    # Build main library
    if [ "$1" != "--run-only" ]; then
        build_main_library
        
        # Build comprehensive tests
        if ! build_comprehensive_tests; then
            print_error "Failed to build comprehensive tests"
            exit 1
        fi
    fi
    
    if [ "$1" = "--build-only" ]; then
        print_success "Build completed successfully"
        exit 0
    fi
    
    # Run tests
    if ! run_all_tests; then
        print_error "Test suite failed"
        exit 1
    fi
    
    print_success "Comprehensive test suite completed successfully!"
}

# Run main function with all arguments
main "$@"