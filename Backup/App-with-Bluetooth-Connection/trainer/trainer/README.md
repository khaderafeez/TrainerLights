# Trainer Lights

Trainer Lights is a Flutter application designed to track and manage training sessions and athlete data. This app provides an intuitive interface for athletes and coaches to monitor performance metrics and improve training outcomes.

## Features

- **Session Tracking**: Record and analyze training sessions with metrics such as total hits, misses, and response times.
- **Athlete Management**: Manage athlete profiles, including their training history and performance metrics.
- **User-Friendly Interface**: A clean and professional UI that enhances user experience.
- **Data Persistence**: Store and retrieve session and athlete data using a local database.

## Project Structure

```
trainer
├── lib
│   ├── main.dart                # Entry point of the application
│   ├── models
│   │   └── db_models.dart       # Data models for sessions and athletes
│   ├── screens
│   │   ├── home_screen.dart      # Main screen displaying overview
│   │   ├── session_screen.dart    # Detailed session information
│   │   └── athlete_screen.dart    # Detailed athlete information
│   ├── widgets
│   │   ├── session_card.dart      # Reusable session card widget
│   │   └── athlete_card.dart      # Reusable athlete card widget
│   ├── services
│   │   └── database_service.dart   # Database operations
│   └── theme
│       └── app_theme.dart         # Application theme settings
├── pubspec.yaml                  # Project configuration and dependencies
└── README.md                     # Project documentation
```

## Installation

1. Clone the repository:
   ```
   git clone <repository-url>
   ```
2. Navigate to the project directory:
   ```
   cd trainer
   ```
3. Install the dependencies:
   ```
   flutter pub get
   ```

## Usage

To run the application, use the following command:
```
flutter run
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request for any enhancements or bug fixes.

## License

This project is licensed under the MIT License. See the LICENSE file for details.