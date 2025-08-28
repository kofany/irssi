#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
print_success() { echo -e "${GREEN}✅ $1${NC}"; }
print_warning() { echo -e "${YELLOW}⚠️  $1${NC}"; }
print_error() { echo -e "${RED}❌ $1${NC}"; }

check_binary() {
    local app_name="$1"
    local binary_path=$(command -v "$app_name" 2>/dev/null)

    if [[ -n "$binary_path" ]]; then
        print_success "$app_name found at: $binary_path"

        # Get version info
        local version=$($app_name --version 2>/dev/null | head -n1)
        if [[ -n "$version" ]]; then
            echo "  Version: $version"
        fi

        # Check if it's our installation
        if [[ "$binary_path" == *"/.local/"* ]] || [[ "$binary_path" == "/opt/"* ]]; then
            echo "  Type: Custom installation"
        else
            echo "  Type: System installation"
        fi

        return 0
    else
        print_warning "$app_name not found in PATH"
        return 1
    fi
}

check_config_directory() {
    local app_name="$1"
    local config_dir=""

    if [[ "$app_name" == "erssi" ]]; then
        config_dir="$HOME/.erssi"
    else
        config_dir="$HOME/.irssi"
    fi

    if [[ -d "$config_dir" ]]; then
        print_success "Config directory exists: $config_dir"

        # Check for main config file
        if [[ -f "$config_dir/config" ]]; then
            echo "  ✓ Main config file found"
        else
            print_warning "  Main config file missing"
        fi

        # Check for scripts directory
        if [[ -d "$config_dir/scripts" ]]; then
            local script_count=$(find "$config_dir/scripts" -name "*.pl" | wc -l)
            echo "  ✓ Scripts directory ($script_count Perl scripts)"
        fi

        # Check for logs directory
        if [[ -d "$config_dir/logs" ]]; then
            echo "  ✓ Logs directory exists"
        fi

        return 0
    else
        print_warning "Config directory not found: $config_dir"
        print_info "Will be created on first run"
        return 1
    fi
}

check_dependencies() {
    print_info "Checking runtime dependencies..."

    local deps=("meson" "ninja" "pkg-config" "perl")
    local missing=()

    for dep in "${deps[@]}"; do
        if command -v "$dep" >/dev/null 2>&1; then
            print_success "$dep available"
        else
            print_warning "$dep not found"
            missing+=("$dep")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        print_warning "Missing dependencies: ${missing[*]}"
        return 1
    else
        print_success "All build dependencies available"
        return 0
    fi
}

check_libraries() {
    print_info "Checking library support..."

    # Check if we can find the installed binary
    local app_name="$1"
    local binary_path=$(command -v "$app_name" 2>/dev/null)

    if [[ -z "$binary_path" ]]; then
        print_warning "Cannot check library support - $app_name not found"
        return 1
    fi

    # Try to run with --version to see if it works
    if $app_name --version >/dev/null 2>&1; then
        print_success "$app_name binary is functional"
    else
        print_error "$app_name binary has issues"
        return 1
    fi

    # Check for common library issues on macOS
    if [[ "$(uname -s)" == "Darwin" ]]; then
        if otool -L "$binary_path" >/dev/null 2>&1; then
            local missing_libs=$(otool -L "$binary_path" | grep -v ":" | grep -c "not found" || true)
            if [[ "$missing_libs" -eq 0 ]]; then
                print_success "All dynamic libraries linked correctly"
            else
                print_error "$missing_libs missing dynamic libraries"
            fi
        fi
    fi
}

show_installation_summary() {
    echo ""
    echo "============================================"
    print_info "Installation Summary"
    echo "============================================"

    # Check both irssi and erssi
    for app in "irssi" "erssi"; do
        echo ""
        print_info "Checking $app..."

        if check_binary "$app"; then
            check_config_directory "$app"
            check_libraries "$app"
        fi
    done
}

show_system_info() {
    echo ""
    echo "============================================"
    print_info "System Information"
    echo "============================================"

    echo "OS: $(uname -s) $(uname -r)"
    echo "Architecture: $(uname -m)"

    # Package manager
    if command -v brew >/dev/null 2>&1; then
        echo "Package Manager: Homebrew $(brew --version | head -n1)"
    elif command -v apt-get >/dev/null 2>&1; then
        echo "Package Manager: APT"
    elif command -v dnf >/dev/null 2>&1; then
        echo "Package Manager: DNF"
    elif command -v pacman >/dev/null 2>&1; then
        echo "Package Manager: Pacman"
    fi

    # Shell
    echo "Shell: $SHELL"

    # PATH info
    if [[ ":$PATH:" == *":$HOME/.local/bin:"* ]]; then
        print_success "~/.local/bin is in PATH"
    else
        print_warning "~/.local/bin is not in PATH"
        print_info "Add to shell profile: export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi
}

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help     Show this help message"
    echo "  -q, --quiet    Quiet mode (less output)"
    echo "  -v, --verbose  Verbose mode (more output)"
    echo ""
    echo "This script checks the status of irssi/erssi installations"
    echo "and verifies that all dependencies are properly configured."
}

main() {
    local quiet=false
    local verbose=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -q|--quiet)
                quiet=true
                shift
                ;;
            -v|--verbose)
                verbose=true
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done

    if [[ "$quiet" != "true" ]]; then
        echo "🔍 Irssi/Erssi Installation Checker"
        echo "=================================="
    fi

    show_system_info
    show_installation_summary

    echo ""
    check_dependencies

    if [[ "$verbose" == "true" ]]; then
        echo ""
        print_info "Environment Variables:"
        echo "PATH: $PATH"
        if [[ -n "$PKG_CONFIG_PATH" ]]; then
            echo "PKG_CONFIG_PATH: $PKG_CONFIG_PATH"
        fi
    fi

    echo ""
    print_info "Check completed!"
}

# Run main function if script is executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
