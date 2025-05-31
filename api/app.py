from fastapi import FastAPI, Request, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from fastapi import Query
from pydantic import BaseModel
from datetime import datetime, timedelta
from tinydb import TinyDB, Query as Q
from operator import itemgetter
import requests
import re

app = FastAPI()

# Add this right after creating your FastAPI app
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Allows all origins
    allow_credentials=True,
    allow_methods=["*"],  # Allows all methods
    allow_headers=["*"],  # Allows all headers
)

db = TinyDB("db.json")
settings_table = db.table("settings")
readings_table = db.table("readings")

class SensorData(BaseModel):
    temperature: float
    presence: bool

class Settings(BaseModel):
    user_temp: float
    user_light: str  # 'sunset' or 'HH:MM:SS'
    light_duration: str  # '4h'

def parse_time(time_str):
    regex = re.compile(r'((?P<hours>\d+?)h)?((?P<minutes>\d+?)m)?')
    parts = regex.match(time_str)
    time_params = {k: int(v) for k, v in parts.groupdict().items() if v}
    return timedelta(**time_params)

def get_sunset_time():
    # Dummy value for now, should use location-based API in production
    return datetime.now().replace(hour=17, minute=45, second=0)

@app.put("/settings")
def set_settings(s: Settings):
    light_on_time = get_sunset_time() if s.user_light == "sunset" else datetime.strptime(s.user_light, "%H:%M:%S")
    light_off_time = (light_on_time + parse_time(s.light_duration)).time()

    doc = {
        "user_temp": s.user_temp,
        "user_light": light_on_time.time().strftime("%H:%M:%S"),
        "light_time_off": light_off_time.strftime("%H:%M:%S"),
    }
    settings_table.truncate()
    settings_table.insert(doc)
    return doc

@app.post("/update")
def update_state(data: SensorData):
    now = datetime.now()
    settings = settings_table.all()[0]

    light_on = settings["user_light"]
    light_off = settings["light_time_off"]
    user_temp = settings["user_temp"]

    current_time = now.time()
    is_light_time = light_on <= current_time.strftime("%H:%M:%S") <= light_off
    should_light = data.presence and is_light_time
    should_fan = data.presence and data.temperature >= user_temp

    readings_table.insert({
        "temperature": data.temperature,
        "presence": data.presence,
        "datetime": now.isoformat()
    })
    
    print("Saved reading:", data.temperature, data.presence, now.isoformat())
    return {"fan": should_fan, "light": should_light}

@app.get("/graph")
def get_graph(size: int = Query(10)):
    all_data = readings_table.all()
    sliced_data = all_data[-size:]

    # Ensure datetime and other fields are JSON-serializable
    safe_data = []
    for item in sliced_data:
        safe_data.append({
            "temperature": item.get("temperature"),
            "presence": item.get("presence"),
            "datetime": str(item.get("datetime"))  # ensure it's a string
        })

    return JSONResponse(content=safe_data)
