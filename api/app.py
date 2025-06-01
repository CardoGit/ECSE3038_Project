from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from datetime import datetime, time, timedelta
from typing import Optional, List
import uvicorn
from collections import deque
import random

app = FastAPI()

# Enable CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# In-memory database (replace with real database in production)
current_settings = {
    "user_temp": 25.0,  # Default temperature trigger
    "user_light": "18:30:00",
    "light_time_off": "22:30:00",
    "_id": "1"
}

# Store historical data (circular buffer)
MAX_HISTORY = 1000  # Maximum number of records to store
sensor_history = deque(maxlen=MAX_HISTORY)

# Data model for settings
class Settings(BaseModel):
    user_temp: float
    user_light: str
    light_duration: str

# Data model for sensor readings
class SensorReading(BaseModel):
    temperature: float
    presence: bool
    datetime: str

# Endpoint to get current settings
@app.get("/settings")
async def get_settings():
    return current_settings

# Endpoint to update settings
@app.put("/settings")
async def update_settings(settings: Settings):
    # Update temperature trigger
    current_settings["user_temp"] = settings.user_temp
    
    # Handle light time (simplified - sunset calculation would go here)
    if settings.user_light.lower() == "sunset":
        # In a real implementation, you'd call a sunset API here
        sunset_time = "18:45:00"  # Placeholder
        current_settings["user_light"] = sunset_time
    else:
        current_settings["user_light"] = settings.user_light
    
    # Calculate light off time (simplified)
    duration = settings.light_duration
    try:
        # Parse duration (simplified version of your regex example)
        if "h" in duration:
            hours = int(duration.split("h")[0])
            off_time = add_hours_to_time(current_settings["user_light"], hours)
            current_settings["light_time_off"] = off_time
    except:
        current_settings["light_time_off"] = "22:30:00"  # Default if parsing fails
    
    return current_settings

# Endpoint to receive sensor data from ESP32
@app.post("/sensor-data")
async def receive_sensor_data(reading: SensorReading):
    # Add new reading to history
    sensor_history.append({
        "temperature": reading.temperature,
        "presence": reading.presence,
        "datetime": reading.datetime
    })
    return {"status": "success"}

# Endpoint to get historical data for graphing
@app.get("/graph")
async def get_graph_data(size: int = Query(10, gt=0, le=MAX_HISTORY)):
    # Return the most recent 'size' readings
    recent_readings = list(sensor_history)[-size:]
    return recent_readings

def add_hours_to_time(time_str: str, hours: int) -> str:
    """Helper function to add hours to a time string"""
    try:
        t = datetime.strptime(time_str, "%H:%M:%S").time()
        new_hour = (t.hour + hours) % 24
        return f"{new_hour:02d}:{t.minute:02d}:{t.second:02d}"
    except:
        return "22:30:00"  # Default if parsing fails

if __name__ == "__main__":
    # Initialize with some dummy data for testing
    for i in range(24):
        sensor_history.append({
            "temperature": random.uniform(20.0, 30.0),
            "presence": random.choice([True, False]),
            "datetime": (datetime.now() - timedelta(hours=i)).isoformat()
        })
    
    uvicorn.run(app, host="0.0.0.0", port=8000)
