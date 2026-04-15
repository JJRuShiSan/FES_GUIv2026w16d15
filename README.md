# FES Thesis GUI

## Project Overview
The FES Thesis GUI is a Qt-based application designed for managing and controlling functional electrical stimulation (FES) sessions. The application features a user-friendly interface that allows users to set parameters, start sessions, and view session history.

## File Structure
The project consists of the following files:

- **src/main.cpp**: Entry point of the application. Initializes the QApplication and displays the StartWindow in full screen.
- **src/mainwindow.cpp**: Implements the MainWindow class, managing the main interface, including amplitude control and navigation to other windows.
- **src/mainwindow.h**: Declares the MainWindow class, its methods, and UI elements.
- **src/electrodewindow.cpp**: Implements the ElectrodeWindow class, managing the electrode matrix interface, button interactions, and session management.
- **src/electrodewindow.h**: Declares the ElectrodeWindow class, its methods, and UI elements.
- **src/sessionwindow.cpp**: Implements the SessionWindow class, managing the session timer and user interactions during a session.
- **src/sessionwindow.h**: Declares the SessionWindow class, its methods, and UI elements.
- **src/historywindow.cpp**: Implements the HistoryWindow class, displaying session history and allowing navigation back to other windows.
- **src/historywindow.h**: Declares the HistoryWindow class, its methods, and UI elements.
- **src/startwindow.cpp**: Implements the StartWindow class, displaying the initial screen with a logo and a button to start the session.
- **src/startwindow.h**: Declares the StartWindow class, its methods, and UI elements.
- **src/resources/logo.qrc**: Qt resource file that includes the logo image used in the application.
- **CMakeLists.txt**: Configuration file for CMake, specifying project settings, source files, and required Qt modules.

## Build Instructions
To build the project, ensure you have CMake and Qt installed. Then, follow these steps:

1. Open a terminal and navigate to the project directory.
2. Create a build directory:
   ```
   mkdir build
   cd build
   ```
3. Run CMake to configure the project:
   ```
   cmake ..
   ```
4. Build the project:
   ```
   make
   ```

## Usage
After building the project, you can run the application. The StartWindow will appear, allowing you to start a new session or navigate to other parts of the application.

## License
This project is licensed under the MIT License. See the LICENSE file for more details.