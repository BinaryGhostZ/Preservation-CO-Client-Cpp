# Preservation CO Client — C++

A C++ implementation of the [Preservation CO Client](https://github.com/deklol/Preservation-CO-Client), an open-source Conquer Online 5065 client skeleton.

This project ports the original Rust implementation to modern C++, with the goal of providing a native C++ codebase for research, preservation, experimentation, and further client development.

## What's Here

The project currently provides a client-side foundation for Conquer Online 5065, including:

* Map rendering
* Character rendering
* Equipment rendering
* Character shadows
* Nameplates
* Walking and running
* Jumping
* Collision
* Movement sounds
* Minimap rendering
* Player minimap marker
* Minimap zoom and expand controls
* Readers for original Conquer Online client files
* Support for the original client asset formats

This is a client skeleton and does not represent the complete Preservation Conquer client.

## Original Project

This project is based on the work of **deklol / digitalm1nd** and the original:

**Preservation-CO-Client**
https://github.com/deklol/Preservation-CO-Client

The original project is an open-source Conquer Online 5065 skeleton client written in Rust.

The original project provides the foundation for the client functionality being ported to C++.

## Requirements

### Windows

* Windows 10/11
* C++23 compatible compiler
* CMake
* Windows SDK
* Git

The project is currently developed and tested on Windows x64.

## Building

Clone the repository:

```bash
git clone <YOUR-REPOSITORY-URL>
cd <YOUR-REPOSITORY>
```

Configure the project:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build --config Release
```

## Client Assets

This repository does **not** include Conquer Online game assets.

You must provide your own legally obtained Conquer Online 5065 client files.

The asset directory should contain the required original client resources, such as:

```text
ini/
map/
data.wdf
c3.wdf
```

Keep the original game assets separate from this source repository.

Example:

```text
C:\Conquer5065\
├── ini\
├── map\
├── data.wdf
└── c3.wdf
```

Then provide the asset directory to the client using the appropriate launch option.

## Controls

| Input               | Action                              |
| ------------------- | ----------------------------------- |
| Left click / hold   | Move                                |
| Ctrl + click / hold | Jump                                |
| `/`                 | Toggle run/walk                     |
| Shift + click       | Turn                                |
| Space               | Stop after the current step or jump |
| Escape              | Exit                                |

Additional controls may be added as development continues.

## Project Status

This project is under active development.

The C++ implementation is intended to preserve the behavior and functionality of the original client skeleton while providing a modern native C++ architecture suitable for further development.

Expect incomplete systems, experimental implementations, and breaking changes while the project is being developed.

## Contributing

Contributions are welcome.

Areas where contributions are particularly useful include:

* Rendering
* Map loading
* Asset readers
* Character animation
* Movement
* Collision
* Minimap
* UI
* Platform support
* Testing
* Documentation

Keep changes focused and document significant architectural changes.

## License

This project is distributed under the **Apache License 2.0**.

It is derived from the original Preservation CO Client, which is also licensed under Apache License 2.0.

See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE) for the applicable license and attribution requirements.

### Original Project Credit

Original Preservation CO Client:

**deklol / digitalm1nd**

https://github.com/deklol/Preservation-CO-Client

The original source code is licensed under Apache License 2.0.

Game assets, client files, trademarks, and other third-party materials are **not** covered by this source-code license and are not distributed with this repository.

## Disclaimer

This project is an independent open-source client implementation for research, preservation, and educational purposes.

Conquer Online and related intellectual property belong to their respective owners.

No original game assets are distributed with this repository.
