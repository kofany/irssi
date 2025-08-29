#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Print functions
print_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
print_success() { echo -e "${GREEN}✅ $1${NC}"; }
print_warning() { echo -e "${YELLOW}⚠️  $1${NC}"; }
print_error() { echo -e "${RED}❌ $1${NC}"; }

detect_system() {
   case "$(uname -s)" in
       Darwin*) echo "macos" ;;
       Linux*) echo "linux" ;;
       *) print_error "Unsupported system: $(uname -s)"; exit 1 ;;
   esac
}

detect_package_manager() {
   local system="$1"
   if [[ "$system" == "macos" ]]; then
       if command -v brew >/dev/null 2>&1; then
           echo "brew"
       else
           print_error "Homebrew not found. Please install Homebrew first: https://brew.sh/"
           exit 1
       fi
   else
       if command -v apt-get >/dev/null 2>&1; then
           echo "apt"
       elif command -v dnf >/dev/null 2>&1; then
           echo "dnf"
       elif command -v pacman >/dev/null 2>&1; then
           echo "pacman"
       else
           print_error "Unsupported package manager. Please install dependencies manually."
           exit 1
       fi
   fi
}

install_dependencies() {
   local system="$1"
   local pkg_mgr="$2"

   print_info "Installing dependencies for $system using $pkg_mgr..."

   case "$pkg_mgr" in
       "brew")
           brew install meson ninja pkg-config glib openssl@3 ncurses utf8proc libgcrypt libotr perl || {
               print_error "Failed to install dependencies with brew"
               exit 1
           }
           ;;
       "apt")
           sudo apt-get update || {
               print_error "Failed to update package list"
               exit 1
           }
           sudo apt-get install -y meson ninja-build build-essential pkg-config perl \
               libglib2.0-dev libssl-dev libncurses-dev libperl-dev libutf8proc-dev \
               libgcrypt20-dev libotr5-dev libattr1-dev || {
               print_error "Failed to install dependencies with apt"
               exit 1
           }
           ;;
       "dnf")
           sudo dnf install -y meson ninja-build gcc pkg-config perl \
               glib2-devel openssl-devel ncurses-devel perl-devel utf8proc-devel \
               libgcrypt-devel libotr-devel libattr-devel || {
               print_error "Failed to install dependencies with dnf"
               exit 1
           }
           ;;
       "pacman")
           sudo pacman -S --needed meson ninja gcc pkg-config perl \
               glib2 openssl ncurses utf8proc libgcrypt libotr || {
               print_error "Failed to install dependencies with pacman"
               exit 1
           }
           ;;
       *)
           print_error "Unknown package manager: $pkg_mgr"
           exit 1
           ;;
   esac

   print_success "Dependencies installed successfully"
}

check_existing_irssi() {
   if command -v irssi >/dev/null 2>&1; then
       local irssi_path=$(which irssi)
       print_warning "Existing irssi installation found at: $irssi_path"
       return 0
   else
       print_info "No existing irssi installation found"
       return 1
   fi
}

# Bezpieczna funkcja yes/no wzorowana na tahioN
yes_or_no() {
   # Sprawdź czy to terminal interaktywny
   if [[ ! -t 0 ]] || [[ ! -t 1 ]]; then
       print_warning "Non-interactive mode - assuming 'no'"
       return 1
   fi
   
   while true; do
       echo -e "${CYAN}$* [y/n]? \c"
       read -n 1 REPLY 2>/dev/null || {
           echo ""
           print_warning "Failed to read input - assuming 'no'"
           return 1
       }
       echo -e "\n"
       case "$REPLY" in
           Y|y) return 0 ;;
           N|n) 
               print_warning "Operation cancelled by user"
               return 1 
               ;;
       esac
   done
}

ask_installation_type() {
   # Sprawdź czy to terminal interaktywny
   if [[ ! -t 0 ]] || [[ ! -t 1 ]]; then
       print_info "Non-interactive mode - using default: erssi"
       echo "erssi"
       return
   fi

   echo ""
   print_info "Choose installation type:"
   echo "1) Install as 'irssi' (replaces system irssi if exists)"
   echo "2) Install as 'erssi' (independent installation - Evolved irssi)"
   echo ""

   if check_existing_irssi; then
       print_warning "WARNING: Choosing option 1 will REPLACE your existing irssi installation!"
       print_warning "Your existing irssi configuration will be preserved, but the binary will be replaced."
   fi

   while true; do
       echo -e "${CYAN}Enter your choice (1 or 2): \c"
       read -n 1 choice 2>/dev/null || {
           echo ""
           print_warning "Failed to read input - using default: erssi"
           echo "erssi"
           return
       }
       echo -e "\n"
       case "$choice" in
           1) echo "irssi"; return ;;
           2|"") echo "erssi"; return ;;
           *) print_error "Please enter 1 or 2" ;;
       esac
   done
}

ask_installation_location() {
   local app_name="$1"
   
   # Sprawdź czy to terminal interaktywny
   if [[ ! -t 0 ]] || [[ ! -t 1 ]]; then
       print_info "Non-interactive mode - using local installation"
       echo "$HOME/.local"
       return
   fi

   echo ""
   print_info "Choose installation location:"
   echo "1) Global installation to /opt/$app_name (requires sudo)"
   echo "2) Local installation to ~/.local (user only)"
   echo ""

   while true; do
       echo -e "${CYAN}Enter your choice (1 or 2): \c"
       read -n 1 choice 2>/dev/null || {
           echo ""
           print_warning "Failed to read input - using local installation"
           echo "$HOME/.local"
           return
       }
       echo -e "\n"
       case "$choice" in
           1) echo "/opt/$app_name"; return ;;
           2|"") echo "$HOME/.local"; return ;;
           *) print_error "Please enter 1 or 2" ;;
       esac
   done
}

setup_build_environment() {
   local system="$1"

   # Set up environment for macOS if needed
   if [[ "$system" == "macos" ]]; then
       if [[ -d "/opt/homebrew" ]]; then
           export PATH="/opt/homebrew/bin:$PATH"
           export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/ncurses/lib/pkgconfig:$PKG_CONFIG_PATH"
       elif [[ -d "/usr/local/Homebrew" ]]; then
           export PATH="/usr/local/bin:$PATH"
           export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/local/opt/ncurses/lib/pkgconfig:$PKG_CONFIG_PATH"
       fi
   fi
}

convert_to_erssi() {
   print_info "Converting irssi to erssi..."
   
   local system="$1"
   
   # Sprawdź czy potrzebne pliki istnieją
   if [[ ! -f "meson.build" ]]; then
       print_error "meson.build not found for erssi conversion"
       exit 1
   fi
   
   if [[ ! -f "src/common.h" ]]; then
       print_error "src/common.h not found for erssi conversion"
       exit 1
   fi
   
   # 1. w meson.build: incdir = 'irssi' -> incdir = 'erssi'
   if [[ "$system" == "macos" ]]; then
       sed -i '' "s/incdir              = 'irssi'/incdir              = 'erssi'/" meson.build || {
           print_error "Failed to update meson.build"
           exit 1
       }
   else
       sed -i "s/incdir              = 'irssi'/incdir              = 'erssi'/" meson.build || {
           print_error "Failed to update meson.build"
           exit 1
       }
   fi

   # 2. w src/common.h: .irssi -> .erssi
   if [[ "$system" == "macos" ]]; then
       sed -i '' 's/\.irssi/\.erssi/g' src/common.h || {
           print_error "Failed to update src/common.h"
           exit 1
       }
   else
       sed -i 's/\.irssi/\.erssi/g' src/common.h || {
           print_error "Failed to update src/common.h"
           exit 1
       }
   fi

   # 3. GLOBALNA zmiana: #include <irssi/ -> #include <erssi/
   if [[ "$system" == "macos" ]]; then
       find . -type f | xargs sed -i '' 's|<irssi/|<erssi/|g' || {
           print_error "Failed to update include paths"
           exit 1
       }
   else
       find . -type f | xargs sed -i 's|<irssi/|<erssi/|g' || {
           print_error "Failed to update include paths"
           exit 1
       }
   fi

   # 4. GLOBALNA zmiana: ".irssi/" -> ".erssi/" (tylko z slashem)
   if [[ "$system" == "macos" ]]; then
       find . -type f | xargs sed -i '' 's|\.irssi/|\.erssi/|g' || {
           print_error "Failed to update directory paths"
           exit 1
       }
   else
       find . -type f | xargs sed -i 's|\.irssi/|\.erssi/|g' || {
           print_error "Failed to update directory paths"
           exit 1
       }
   fi

   # 5. w src/fe-text/meson.build: executable('irssi', -> executable('erssi',
   if [[ -f "src/fe-text/meson.build" ]]; then
       if [[ "$system" == "macos" ]]; then
           sed -i '' "s/executable('irssi',/executable('erssi',/" src/fe-text/meson.build || {
               print_error "Failed to update executable name"
               exit 1
           }
       else
           sed -i "s/executable('irssi',/executable('erssi',/" src/fe-text/meson.build || {
               print_error "Failed to update executable name"
               exit 1
           }
       fi
   else
       print_warning "src/fe-text/meson.build not found - skipping executable name change"
   fi

   print_success "Successfully converted to erssi - exactly 5 changes made"
}

build_and_install() {
   local app_name="$1"
   local install_path="$2"
   local system="$3"

   print_info "Building $app_name..."

   # Clean previous builds
   if [[ -d "Build" ]]; then
       rm -rf Build
   fi

   # Setup meson build
   local meson_args=(
       "--prefix=$install_path"
       "-Dwith-perl=yes"
       "-Dwith-otr=yes"
       "-Ddisable-utf8proc=no"
   )

   print_info "Setting up build with: meson setup Build ${meson_args[*]}"
   meson setup Build "${meson_args[@]}" || {
       print_error "Meson setup failed"
       exit 1
   }

   print_info "Building with ninja..."
   ninja -C Build || {
       print_error "Build failed"
       exit 1
   }

   print_info "Installing $app_name to $install_path..."

   if [[ "$install_path" == "/opt/"* ]]; then
       sudo ninja -C Build install || {
           print_error "Installation failed"
           exit 1
       }
   else
       ninja -C Build install || {
           print_error "Installation failed"
           exit 1
       }
   fi

   print_success "$app_name built and installed successfully"
}

create_symlinks() {
   local app_name="$1"
   local install_path="$2"

   local bin_path="$install_path/bin/$app_name"

   if [[ ! -f "$bin_path" ]]; then
       print_error "Binary not found at $bin_path"
       return 1
   fi

   # Create symlink in user's local bin if it's a global install
   if [[ "$install_path" == "/opt/"* ]]; then
       mkdir -p "$HOME/.local/bin" || {
           print_error "Failed to create $HOME/.local/bin"
           return 1
       }
       
       ln -sf "$bin_path" "$HOME/.local/bin/$app_name" || {
           print_error "Failed to create symlink"
           return 1
       }
       
       print_success "Created symlink: $HOME/.local/bin/$app_name -> $bin_path"

       # Add to PATH if not already there
       if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
           print_info "Add to your shell profile: export PATH=\"\$HOME/.local/bin:\$PATH\""
       fi
   fi
}

show_completion_message() {
   local app_name="$1"
   local install_path="$2"

   echo ""
   print_success "🎉 $app_name installation completed!"
   echo ""
   print_info "Installation details:"
   echo "  • Application: $app_name"
   echo "  • Location: $install_path"
   echo "  • Binary: $install_path/bin/$app_name"
   echo ""

   if [[ "$app_name" == "erssi" ]]; then
       print_info "Erssi-specific features:"
       echo "  • Configuration directory: ~/.erssi/"
       echo "  • All irssi functionality with evolved features"
   fi

   print_info "To run: $app_name"

   if [[ "$install_path" == "/opt/"* ]] && [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
       echo ""
       print_warning "Note: Add ~/.local/bin to your PATH to run $app_name from anywhere:"
       echo "  echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ~/.bashrc"
       echo "  source ~/.bashrc"
   fi
}

# Cleanup function
cleanup() {
   if [[ -d "Build" ]]; then
       print_info "Cleaning up temporary files..."
       rm -rf Build
   fi
}

# Setup trap for cleanup
trap cleanup EXIT INT TERM

main() {
   echo "🚀 Advanced Irssi/Erssi Installation Script"
   echo "=========================================="

   # Detect system and package manager
   local system
   system=$(detect_system) || exit 1
   
   local pkg_mgr
   pkg_mgr=$(detect_package_manager "$system") || exit 1

   print_info "Detected system: $system"
   print_info "Package manager: $pkg_mgr"

   # Check if we're in the right directory
   if [[ ! -f "meson.build" ]]; then
       print_error "meson.build not found. Please run this script from the irssi source directory."
       exit 1
   fi

   # Ask for confirmation
   if ! yes_or_no "Do you want to proceed with installation?"; then
       exit 1
   fi

   # Install dependencies
   print_info "Checking and installing dependencies..."
   install_dependencies "$system" "$pkg_mgr"

   # Setup build environment
   setup_build_environment "$system"

   # Ask user preferences
   local app_name
   app_name=$(ask_installation_type)
   
   local install_path
   install_path=$(ask_installation_location "$app_name")

   # Convert to erssi if needed
   if [[ "$app_name" == "erssi" ]]; then
       convert_to_erssi "$system"
   fi

   # Build and install
   build_and_install "$app_name" "$install_path" "$system"

   # Create symlinks if needed
   create_symlinks "$app_name" "$install_path"

   # Show completion message
   show_completion_message "$app_name" "$install_path"

   echo ""
   print_success "Installation complete! 🎉"
}

# Check if script is being sourced or executed
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
   main "$@"
fi