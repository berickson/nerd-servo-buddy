# nerd-servo-buddy

Bus servo configuration and utility targeting the Waveshare ESP32 Servo Driver Expansion Board.

This is designed for getting the servos ready for your projects by setting ids, ranges, safety limits, etc. and providing a quick control interface to manually control them.

It auto detects SCS and STS style servos.

Features:

- Web-based servo control dashboard
- Auto-detect up to 253 servos
- Real-time telemetry and alarms
- Wi-Fi AP or station mode
- Mixed SC/STS servo support
- Position, speed, acceleration control
- Persistent servo names and preferences
- Angle limits and torque safety
- RGB LED status indicator
- JSON API for programmatic access