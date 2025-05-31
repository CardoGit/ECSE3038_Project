# ECSE3038_Project - Simple Smart Hub

An IoT platform that controls appliances based on time, temperature, and presence detection.

## Project Structure

- `api/`: Contains the FastAPI backend code
  - `app.py`: Main FastAPI application
- `src/`: Contains the ESP32 firmware
  - `main.cpp`: Main firmware code
- `test/`: Test files
- `requirements.txt`: Python dependencies
- `platformio.ini`: PlatformIO configuration for ESP32

## Setup Instructions

### Backend Setup (Ubuntu)

1. Install Python 3.8+ if not already installed
2. Create and activate a virtual environment:
   ```bash
   python3 -m venv venv
   source venv/bin/activate
