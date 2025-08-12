# Irssi

## Project Overview

Irssi is a modular and extensible chat client with a text-based user interface. It is written in C and uses the GLib library for its main loop and data structures. Irssi supports multiple chat protocols through a modular plugin system, with IRC being the primary supported protocol.

The architecture of Irssi is based on a `CORE` module that provides fundamental services like signal handling, configuration management, and networking. On top of the core, there are modules for different chat protocols (e.g., IRC, XMPP) and user interfaces (e.g., text-based, graphical). This modular design allows for easy extension and customization.

Communication between modules is handled through a signal/event system, which allows for loose coupling and dynamic loading of plugins.

## Building and Running

Irssi uses the Meson build system.

### Dependencies

*   GLib >= 2.32
*   OpenSSL
*   Perl >= 5.8
*   Terminfo or Ncurses

### Building from source

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/irssi/irssi
    cd irssi
    ```

2.  **Configure the build:**
    ```bash
    meson Build
    ```

3.  **Compile the source:**
    ```bash
    ninja -C Build
    ```

4.  **Install Irssi:**
    ```bash
    sudo ninja -C Build install
    ```

### Running Irssi

Once installed, you can run Irssi by simply typing `irssi` in your terminal.

```bash
irssi
```

## Development Conventions

### Coding Style

The project follows the coding style defined in the `.clang-format` file. Developers should use `clang-format` to format their code before submitting patches.

### Testing

The project has a `tests` directory that contains a suite of tests. To run the tests, you can use the following command:

```bash
ninja -C Build test
```

### Contribution Guidelines

Contributions are welcome and should be submitted as pull requests on GitHub. Before submitting a pull request, please make sure that your code is well-formatted, tested, and follows the project's coding style.
