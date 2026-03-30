# Trainer App

## Overview
The Trainer app is a Flutter application designed to track and manage athlete sessions. It provides a user-friendly interface for viewing and managing session data and athlete information.

## Project Structure
```
trainer
├── lib
│   ├── main.dart                # Entry point of the application
│   ├── models
│   │   └── db_models.dart       # Data models for sessions and athletes
│   ├── screens
│   │   ├── home_screen.dart      # Main interface for navigation
│   │   ├── session_screen.dart    # Displays session-related information
│   │   └── athlete_screen.dart    # Displays athlete-related information
│   ├── widgets
│   │   └── custom_button.dart     # Reusable custom button widget
│   ├── services
│   │   └── database_service.dart   # Handles database operations
│   └── utils
│       └── constants.dart         # Contains constant values used throughout the app
├── pubspec.yaml                  # Project configuration and dependencies
├── analysis_options.yaml         # Dart analysis options
├── README.md                     # Project documentation
└── .gitignore                    # Files to ignore in version control
```

## Setup Instructions
1. Clone the repository:
   ```
   git clone <repository-url>
   ```
2. Navigate to the project directory:
   ```
   cd TrainerLights
   ```
3. Install dependencies:
   ```
   flutter pub get
   ```
4. Run the application:
   ```
   flutter run
   ```

## Usage
- Launch the app to access the home screen.
- Navigate to the session screen to view and manage session data.
- Access the athlete screen to view and manage athlete information.

## Contributing
Contributions are welcome! Please submit a pull request or open an issue for any enhancements or bug fixes.

## License
This project is licensed under the MIT License. See the LICENSE file for details.