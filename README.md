# Span - Cross-Language Dependency Management

Span is a high-performance dependency management tool. It provides a unified interface for managing project dependencies across different languages and ecosystems, with a focus on speed and efficiency through local caching and concurrent operations.

## Key Features

- **Modern C++23 codebase**: Utilizes the latest C++ standards for performance and safety.
- **Extensible architecture**: Easily add support for new package managers by implementing the `Manager` interface.
- **Concurrent operations**: Uses a thread pool to install and link multiple packages in parallel.
- **Efficient caching**: Maintains a local cache of packages to avoid redundant downloads and speed up subsequent installations.
- **High-speed parsing**: Integrates the `simdjson` library for fast parsing of dependency files.
- **Cross-platform**: Designed to work on Windows, macOS, and Linux.

## Requirements

- A C++23 compatible compiler (e.g., GCC 11+, Clang 14+, MSVC 19.29+).
- CMake (version 3.23 or higher).
- Git.

## Building the Project

1.  **Clone the repository:**
    ```bash
    git clone <repository-url>
    cd dev
    ```

2.  **Configure the build:**
    ```bash
    cmake -B build -S .
    ```
    You can enable sanitizers for debugging by adding `-DENABLE_SANITIZERS=ON`.

3.  **Compile the project:**
    ```bash
    cmake --build build
    ```

The main executable `dev` will be located in the `build/` directory.

## Usage

The tool is designed to be run from the command line. You can install dependencies for a project by pointing to its directory.

**Example:**
```bash
./build/dev install
```

The tool will automatically detect the project type (e.g., Composer) and begin installing its dependencies using the local cache.

## Contributing

To add support for a new package manager:

1.  Create your new manager class header and implementation in `include/packages/` and `src/packages/` respectively. Ensure your class inherits from `dev::packages::Manager`.
2.  Implement the required virtual methods:
    - `isProjectType()`: Detect if a directory is a valid project for your manager.
    - `getManagerName()`: Return the name of the language/ecosystem (e.g., "composer").
    - `getInstallDirectory()`: Return the name of the dependency directory (e.g., "vendor").
    - `getDependencyFiles()`: Return a list of files that define dependencies (e.g., `package.json`).
    - `getInstalledVersions()`: Return a map of packages and their versions, usually parsed from a lock file.
    - `installDependency()`: Logic for downloading and caching a single package.

That's it! The build system will automatically detect your new files, generate the necessary registration code, and include it in the final executable. There is no need to manually edit any other files to register your manager.

## Dependencies

This project relies on the following third-party libraries, which are included as submodules or fetched by CMake:

- **[simdjson](https://github.com/simdjson/simdjson)**: A high-performance JSON parsing library.
- **[CLI11](https://github.com/CLIUtils/CLI11)**: A powerful command-line parser for C++.
