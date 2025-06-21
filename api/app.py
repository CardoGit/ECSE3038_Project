from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from datetime import datetime, time, timedelta
from typing import List, Optional
import re
import uvicorn
from collections import deque
from astral import LocationInfo
from astral.sun import sun
import pytz

app = FastAPI()

# Enable CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Updated for Jamaica
LOCATION = LocationInfo(
    timezone="America/Jamaica",
    latitude=18.1096,   # Jamaica's approximate latitude
    longitude=-77.2975, # Jamaica's approximate longitude
    name="Jamaica"
)

# Dictionary to simulate settings database
current_settings = {
    "user_temp": 25.0,
    "user_light": "18:30:00",
    "light_time_off": "22:30:00",
    "_id": "1",
    "original_light": "18:30:00"
}

sensor_data = deque(maxlen=1000)

class SensorReading(BaseModel):
    temperature: float
    presence: bool
    timestamp: str

def get_sunset_time():
    """Get today's sunset time for Jamaica"""
    try:
        tz = pytz.timezone(LOCATION.timezone)
        today = datetime.now(tz).date()
        
        s = sun(LOCATION.observer, date=today, tzinfo=tz)
        
        # Convert to Jamaica time and format
        sunset_jamaica = s["sunset"].astimezone(tz)
        return sunset_jamaica.strftime("%H:%M:%S")
    except Exception as e:
        print(f"Error calculating sunset: {e}")
        return "18:45:00"  # Fallback value

@app.post("/temperature")
async def receive_temperature(reading: SensorReading):
    try:
        record = {
            "temperature": float(f"{reading.temperature:.1f}"),
            "presence": reading.presence,
            "timestamp": reading.timestamp,
            "datetime": datetime.now().replace(microsecond=0).isoformat(),
            "server_received": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }
        sensor_data.append(record)
        return {"status": "success"}
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@app.get("/graph")
async def get_graph_data(size: int = Query(10, gt=0, le=1000)):
    recent_data = list(sensor_data)[-size:]
    formatted_data = [{
        "temperature": entry["temperature"],
        "presence": entry["presence"],
        "datetime": entry["datetime"]
    } for entry in recent_data]
    return formatted_data

class Settings(BaseModel):
    user_temp: float
    user_light: str
    light_duration: str

def parse_duration(time_str):
    regex = re.compile(r'((?P<hours>\d+?)h)?((?P<minutes>\d+?)m)?((?P<seconds>\d+?)s)?')
    parts = regex.match(time_str)
    if not parts:
        return None
    parts = parts.groupdict()
    time_params = {}
    for name, param in parts.items():
        if param:
            time_params[name] = int(param)
    return timedelta(**time_params)

@app.get("/settings")
async def get_settings():
    return current_settings

@app.put("/settings")
async def update_settings(settings: Settings):
    current_settings["user_temp"] = settings.user_temp
    
    if settings.user_light.lower() == "sunset":
        sunset_time = get_sunset_time()
        print(f"Calculated sunset time for Jamaica: {sunset_time}")  # Debug output
        current_settings["user_light"] = sunset_time
        current_settings["original_light"] = "sunset"
    else:
        current_settings["user_light"] = settings.user_light
        current_settings["original_light"] = settings.user_light
    
    duration = parse_duration(settings.light_duration)
    if duration:
        try:
            light_on = datetime.strptime(current_settings["user_light"], "%H:%M:%S")
            light_off = (light_on + duration).time()
            current_settings["light_time_off"] = light_off.strftime("%H:%M:%S")
        except:
            current_settings["light_time_off"] = "22:30:00"
    else:
        current_settings["light_time_off"] = "22:30:00"
    
    return current_settings

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
