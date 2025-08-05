#!/bin/bash
# SS P2P Node Controller Docker Development Helper

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Show usage
show_usage() {
    echo "SS P2P Node Controller Docker Development Helper"
    echo ""
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  build           Build the Docker image"
    echo "  dev             Start development container"
    echo "  network         Start full P2P network (stable-host + 4 nodes)"
    echo "  test            Run tests in container"
    echo "  clean           Clean up containers and images"
    echo "  logs [SERVICE]  Show logs for service (default: all)"
    echo "  shell [SERVICE] Connect to running container shell"
    echo "  stop            Stop all running containers"
    echo "  rebuild         Clean and rebuild everything"
    echo ""
    echo "Examples:"
    echo "  $0 build                    # Build Docker image"
    echo "  $0 dev                      # Start development environment"
    echo "  $0 network                  # Start full P2P network"
    echo "  $0 shell node-0             # Connect to node-0 shell"
    echo "  $0 logs stable-host         # Show stable-host logs"
    echo "  $0 test                     # Run tests"
}

# Build Docker image
build_image() {
    log_info "Building SS P2P Docker image..."
    cd "$PROJECT_ROOT"
    docker build -t ss-p2p-node-controller .
    log_success "Docker image built successfully"
}

# Start development environment
start_dev() {
    log_info "Starting development environment..."
    cd "$PROJECT_ROOT"
    docker-compose up -d ss-p2p-dev
    log_success "Development environment started"
    log_info "Connect with: docker exec -it ss_p2p_dev /bin/bash"
}

# Start full P2P network
start_network() {
    log_info "Starting full P2P network..."
    cd "$PROJECT_ROOT"
    docker-compose up -d
    log_success "P2P network started"
    log_info "Services running:"
    docker-compose ps
}

# Run tests
run_tests() {
    log_info "Running tests in container..."
    cd "$PROJECT_ROOT"
    docker-compose run --rm ss-p2p-dev bash -c "cd build && ctest --verbose"
}

# Clean up
cleanup() {
    log_info "Cleaning up containers and images..."
    cd "$PROJECT_ROOT"
    docker-compose down --volumes --remove-orphans
    docker image prune -f
    log_success "Cleanup completed"
}

# Show logs
show_logs() {
    local service=${1:-}
    cd "$PROJECT_ROOT"
    if [[ -z "$service" ]]; then
        log_info "Showing logs for all services..."
        docker-compose logs -f
    else
        log_info "Showing logs for service: $service"
        docker-compose logs -f "$service"
    fi
}

# Connect to shell
connect_shell() {
    local service=${1:-ss-p2p-dev}
    local container_name=""
    
    case "$service" in
        "dev"|"ss-p2p-dev")
            container_name="ss_p2p_dev"
            ;;
        "stable-host")
            container_name="ss_p2p_stable_host"
            ;;
        "node-0")
            container_name="ss_p2p_node_0"
            ;;
        "node-1")
            container_name="ss_p2p_node_1"
            ;;
        "node-2")
            container_name="ss_p2p_node_2"
            ;;
        "node-3")
            container_name="ss_p2p_node_3"
            ;;
        *)
            container_name="$service"
            ;;
    esac
    
    log_info "Connecting to shell: $container_name"
    docker exec -it "$container_name" /bin/bash
}

# Stop all containers
stop_all() {
    log_info "Stopping all containers..."
    cd "$PROJECT_ROOT"
    docker-compose down
    log_success "All containers stopped"
}

# Rebuild everything
rebuild_all() {
    log_info "Rebuilding everything..."
    cleanup
    build_image
    log_success "Rebuild completed"
}

# Main script logic
case "${1:-}" in
    "build")
        build_image
        ;;
    "dev")
        start_dev
        ;;
    "network")
        start_network
        ;;
    "test")
        run_tests
        ;;
    "clean")
        cleanup
        ;;
    "logs")
        show_logs "$2"
        ;;
    "shell")
        connect_shell "$2"
        ;;
    "stop")
        stop_all
        ;;
    "rebuild")
        rebuild_all
        ;;
    "help"|"--help"|"-h")
        show_usage
        ;;
    "")
        log_error "No command specified"
        show_usage
        exit 1
        ;;
    *)
        log_error "Unknown command: $1"
        show_usage
        exit 1
        ;;
esac