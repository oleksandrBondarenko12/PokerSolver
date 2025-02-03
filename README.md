# Poker Solver UI 🎲🃏

## Overview ✨
**Poker Solver UI** is a modern graphical application designed to visualize and interact with advanced poker strategy data. Built with Qt6 and styled with inspiration from Tailwind CSS, this tool presents an intuitive interface that dynamically adapts to your viewport. The project is configured with CMake, ensuring a smooth cross-platform build process.

## Table of Contents 📚
- [Overview](#overview-)
- [Features](#features-)
- [Project Structure](#project-structure-)
- [Installation & Build](#installation--build-)
- [Usage](#usage-)
- [Dependencies](#dependencies-)
- [Contributing](#contributing-)
- [License](#license-)
- [Future Improvements](#future-improvements-)

## Features 🌟
- **Modern UI Design:**  
  Enjoy a sleek, dark-themed interface influenced by Tailwind CSS trends.
- **Dynamic Hand Matrix:**  
  The Strategy Explorer features a responsive 13×13 hand matrix that adjusts gracefully to different screen sizes.
- **Range Selector Dialog:**  
  Easily select and modify card ranges with an intuitive dialog.
- **Strategy Explorer:**  
  Dive deep into poker strategies with interactive visualizations.
- **Cross-Platform Build:**  
  Use CMake to generate build files and compile across various platforms.

## Project Structure 📁

```
PokerSolver/
├── CMakeLists.txt          # CMake build configuration
├── main.cpp                # Application entry point
├── MainWindow.cpp          # Main Window implementation (UI integration)
├── MainWindow.h            # Main Window declaration
├── RangeSelector.cpp       # Dialog implementation for card range selection
├── RangeSelector.h         # Dialog declaration for card range selection
├── StrategyExplorer.cpp    # Implements the Strategy Explorer, including the dynamic hand matrix
├── StrategyExplorer.h      # Declaration for Strategy Explorer
├── resources.qrc           # Bundles external assets (stylesheets, images) for Qt
├── styles
│   └── dark.qss            # Dark theme stylesheet
├── combined.html           # Design reference for combined layout
├── main-window.html        # Design reference for the main window
├── range-selector.html     # Design reference for range selector
├── strategy-explorer.html  # Design reference for strategy explorer
└── .vscode/
    ├── c_cpp_properties.json   # VSCode C/C++ configuration
    ├── launch.json             # Debugging configuration
    └── settings.json           # Workspace settings
```

### File Explanations
- **CMakeLists.txt:**  
  Defines the project settings, source files, and Qt module dependencies.

- **main.cpp:**  
  The entry point of the application. It initializes the Qt framework and launches the Main Window.

- **MainWindow.cpp / MainWindow.h:**  
  Contains the implementation and declaration for the main application window, which integrates various dialogs.

- **RangeSelector.cpp / RangeSelector.h:**  
  Implements a dialog that allows users to select poker hand ranges interactively.

- **StrategyExplorer.cpp / StrategyExplorer.h:**  
  Implements the Strategy Explorer, including a dynamic 13×13 hand matrix that auto-scales with the window size.

- **resources.qrc:**  
  Aggregates all external assets (images, stylesheets) into a single resource file for easier management.

- **styles/dark.qss:**  
  The stylesheet defining the dark theme that gives the application its modern look.

- **HTML Files:**  
  Serve as design references to keep the UI layout and styling consistent.

- **.vscode Directory:**  
  Contains Visual Studio Code configuration files to aid in development with features like IntelliSense and debugging.

## Installation & Build 🔧

Follow these steps to set up and build the project:

1. **Generate the build files:**
   ```bash
   cmake -S . -B build
   ```

2. **Build the project:**
   ```bash
   cmake --build build
   ```

3. **Run the executable:**
   ```bash
   ./build/poker_solver_ui
   ```

*Note: Ensure you have CMake, Qt6, and a C++17 compatible compiler installed.*

## Usage 🎮

- **Main Window:**  
  Acts as the central hub that integrates various dialogs and displays the dynamic hand matrix.

- **Dynamic Hand Matrix:**  
  Accessible via the Strategy Explorer, it adjusts its cell sizes based on the window's dimensions.

- **Range Selector:**  
  A user-friendly dialog that enables the selection and modification of card ranges.

## Dependencies 📦

- **Qt6:**  
  Utilized for building the graphical interface (Modules: Widgets, Core, Gui).

- **C++17:**  
  Leverages C++17 features to ensure modern, efficient code.

- **CMake:**  
  Handles cross-platform configuration and building.

## Contributing 🤝

Contributions are highly welcome! To get started:

1. **Fork the Repository:**  
   Click the "Fork" button at the top right to create your own copy of the project.

2. **Create a Branch:**  
   Use descriptive branch names, e.g., `feature/dynamic-hand-matrix`.

3. **Commit Your Changes:**  
   Make sure your commits are clear and follow the project's coding standards.

4. **Submit a Pull Request:**  
   Provide a thorough description of your changes, referencing any related issues.

For more detailed guidelines, please refer to our [CONTRIBUTING.md](CONTRIBUTING.md) (if available).

## License 📄

This project is licensed under the [Your License Name](LICENSE) *(update with your actual license info)*.  
Feel free to add any additional information regarding licensing as necessary.

## Future Improvements 🔮

- **Solver Logic Integration:**  
  Enhance backend functionalities to deeply integrate the solving engine with the UI.

- **Multi-threading Enhancements:**  
  Optimize performance by implementing advanced multi-threaded operations.

- **Enhanced Visualizations:**  
  Explore interactive data visualizations for game trees, range distributions, and strategy recommendations.

- **User Experience Refinements:**  
  Improve error handling, include loading indicators, and refine the responsive design for even smoother interactions.

---

Thank you for checking out Poker Solver UI!  
Happy coding, and may the odds be ever in your favor! 🎉