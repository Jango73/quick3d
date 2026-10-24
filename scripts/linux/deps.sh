#!/bin/bash
set -euo pipefail

SCRIPT_FOLDER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT_FOLDER="$(cd "${SCRIPT_FOLDER}/../.." && pwd)"

EXIT_SUCCESS=0
EXIT_FAILURE=1

APT_PACKAGES=(
  build-essential
  cmake
  ninja-build
  git
  libgl-dev
  zlib1g-dev
  qt6-base-dev
  qt6-base-dev-tools
  qt6-declarative-dev
  qt6-multimedia-dev
  libqt6serialport6-dev
  qt6-positioning-dev
  qt6-tools-dev
  qt6-tools-dev-tools
)

DNF_PACKAGES=(
  gcc-c++
  cmake
  ninja-build
  git
  mesa-libGL-devel
  zlib-devel
  qt6-qtbase-devel
  qt6-qtdeclarative-devel
  qt6-qtmultimedia-devel
  qt6-qtserialport-devel
  qt6-qtpositioning-devel
  qt6-qttools-devel
)

PACMAN_PACKAGES=(
  base-devel
  cmake
  ninja
  git
  mesa
  zlib
  qt6-base
  qt6-declarative
  qt6-multimedia
  qt6-serialport
  qt6-positioning
  qt6-tools
)

install_with_apt() {
  sudo apt-get update
  sudo apt-get install -y "${APT_PACKAGES[@]}"
}

install_with_dnf() {
  sudo dnf install "${DNF_PACKAGES[@]}"
}

install_with_pacman() {
  sudo pacman -S --needed "${PACMAN_PACKAGES[@]}"
}

main() {
  echo "Initializing submodules..."
  git -C "${PROJECT_ROOT_FOLDER}" submodule update --init --recursive

  if command -v apt-get >/dev/null 2>&1; then
    install_with_apt
    return
  fi

  if command -v dnf >/dev/null 2>&1; then
    install_with_dnf
    return
  fi

  if command -v pacman >/dev/null 2>&1; then
    install_with_pacman
    return
  fi

  echo "Unsupported package manager. Supported package managers: apt-get, dnf, pacman." >&2
  exit 1
}

main
