<p align="center">
  <a href="https://github.com/moesay/e2asm/">
    <img src="https://github.com/moesay/e2asm/blob/main/e2-logo.png" alt="TheE2Project" width="763" height="169">
  </a>
</p>

<h3 align="center">E2Asm</h3>

<p align="center">
  A cross-platform, Intel-syntax compatible 8086 assembler that is not better than NASM.
  <br>
  <a href="https://github.com/moesay/e2asm/issues/new?template=bug_report.md">Report bug</a>
  ·
  <a href="https://github.com/moesay/e2asm/issues/new?template=feature_request.md">Request feature</a>
</p>

<p align="center">
      <a href="https://github.com/moesay/e2asm/blob/master/LICENSE" alt="License">
        <img src="https://img.shields.io/github/license/moesay/e2asm" /></a>
      <a href="https://github.com/moesay/e2asm/" alt="Status">
        <img src="https://img.shields.io/badge/Status-WIP-f10" /></a>
      <a href="https://github.com/moesay/e2asm/" alt="Dev Status">
        <img src="https://img.shields.io/badge/Developing-Active-green" /></a>
      <a href="https://github.com/moesay/e2asm/actions/workflows/build-test.yaml" alt="Status">
        <img src="https://github.com/moesay/e2asm/actions/workflows/build-test.yaml/badge.svg" /></a>
      <a href="https://github.com/moesay/e2asm/" alt="Repo Size">
        <img src="https://img.shields.io/github/repo-size/moesay/e2asm?label=Repository%20size" /></a>
      <a href="https://github.com/moesay/e2asm/issues/" alt="Issues">
        <img src="https://img.shields.io/github/issues/moesay/e2asm" /></a>
      <a href="https://github.com/moesay/e2asm/pulls/" alt="PRs">
        <img src="https://img.shields.io/github/issues-pr/moesay/e2asm" /></a>
 </p>

## Table of contents

- [Why?](#why)
- [Overview](#overview)
- [Quick start](#quick-start)
  - [Linux](#linux)
  - [Windows](#windows)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Bugs and features requests](#bugs-and-features-requests)
- [Copyright and license](#copyright-and-license)

## Why?

E2Asm exists because I'm genuinely interested in the 8086 architecture and wanted to create a modern assembler that others can actually use and build upon. Unlike traditional assemblers that are designed purely as standalone tools, E2Asm is built with **integration in mind** from the ground up.

This means you can:
- **Embed it into your projects** - Use E2Asm as a library in your own applications
- **Build on top of it** - Create custom tooling, IDEs, or educational platforms that leverage the assembler
- **Integrate it into larger systems** - The clean architecture and well-defined interfaces make it easy to incorporate into compilers, emulators, or development environments

Whether you're building a retro computing emulator, creating an educational tool for teaching assembly language, or developing a custom toolchain, E2Asm provides the flexibility and modularity you need.

## Overview

E2Asm is a cross-platform 8086 assembler written in modern C++20. It aims to provide Intel-syntax compatibility while being more accessible than traditional assemblers. The project follows a traditional compiler architecture with distinct phases:

- **Preprocessor**: Handles preprocessing directives and macros
- **Lexer**: Tokenizes the assembly source code
- **Parser**: Builds an Abstract Syntax Tree (AST) from tokens
- **Semantic Analyzer**: Performs semantic analysis and symbol resolution
- **Code Generator**: Generates 8086 machine code with ModR/M encoding

The assembler is designed with clean separation of concerns, making it easier to understand, maintain, and extend.

## Quick start

### Linux

To build and run E2Asm on Linux:

```bash
# Clone the repository
git clone https://github.com/moesay/e2asm.git
cd e2asm

# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build the project
make

# Run the unit tests
make check

# Or run tests with verbose output
make check_verbose
```

**Requirements:**
- CMake 3.14 or higher
- C++20 compatible compiler (GCC 10+, Clang 10+)
- Git (for fetching GoogleTest dependency)

### Windows

To build E2Asm on Windows:

```powershell
# Clone the repository
git clone https://github.com/moesay/e2asm.git
cd e2asm

# Create build directory
mkdir build
cd build

# Configure with CMake (using Visual Studio generator)
cmake ..

# Build the project
cmake --build .

# Run the unit tests
ctest --output-on-failure
```

**Requirements:**
- CMake 3.14 or higher
- Visual Studio 2019 or higher (with C++20 support)
- Git (for fetching GoogleTest dependency)

Alternatively, you can use MinGW-w64 or Clang on Windows following similar steps to the Linux build.

## Documentation

Comprehensive documentation is currently under development. We are in the process of generating API documentation using Doxygen.

In the meantime, the codebase is structured to be self-documenting with clear separation of concerns:
- Check the `core/` directory for the main assembler interface
- Explore `lexer/`, `parser/`, `semantic/`, and `codegen/` for implementation details
- Unit tests in the `test/` directory serve as usage examples

Stay tuned for complete documentation coming soon!

## Contributing

We welcome contributions from developers of all skill levels! For now, the contribution process is straightforward:

**Current guidelines:**
- Ensure your changes pass the build: `make` or `cmake --build .`
- Ensure all unit tests pass: `make check` or `ctest`
- Write unit tests for new functionality when applicable

That's it! As long as the build succeeds and tests pass, your contribution is welcome.

**Note:** More detailed contribution guidelines will be added in the future as the project matures. This will include coding standards, commit message conventions, and code review processes.

To get started:
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Run the tests to ensure everything works
5. Commit your changes (`git commit -m 'Add some amazing feature'`)
6. Push to your branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## Bugs and features requests

Found a bug or have an idea for a new feature? We'd love to hear from you!

- **Bug reports**: [Create a bug report](https://github.com/moesay/e2asm/issues/new?template=bug_report.md)
- **Feature requests**: [Request a feature](https://github.com/moesay/e2asm/issues/new?template=feature_request.md)

Before creating a new issue, please search existing issues to avoid duplicates. When reporting bugs, include as much detail as possible:
- Your operating system and version
- Compiler version
- Steps to reproduce the issue
- Expected vs actual behavior
- Any relevant code snippets or assembly files

## Copyright and license

E2Asm is licensed under the **GNU General Public License v3.0 (GPL-3.0)**.

This means you are free to:
- Use the software for any purpose
- Change the software to suit your needs
- Share the software with your friends and neighbors
- Share the changes you make

Under the condition that:
- You share your modifications under the same GPL-3.0 license
- You include the original copyright notice

For the complete license text, see the [LICENSE](https://github.com/moesay/e2asm/blob/master/LICENSE) file in the repository.

---

<p align="center">Made with love, determination, and a passion for the 8086 processor</p>
<p align="center">Copyright (C) 2019-2026. The E2Asm Contributors</p>
