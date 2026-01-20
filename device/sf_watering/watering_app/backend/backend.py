import json
import time
import random
import os
from flask import Flask, request, jsonify
from flask_cors import CORS
# [FIXED] Updated import to match corrected filename
from sf_watering_mqtt_client import sf_mqtt_watering_client

app = Flask(__name__)
CORS(app)

# --- CONFIGURATION ---
BROKER_ADDRESS = "localhost" 
BROKER_PORT = 1883
BROKER_USERNAME = "user"    
BROKER_PASSWORD = "password" 
TOPIC = "sf_watering/schedule"
SCHEDULES_FILE = "schedules.json"

# Initialize MQTT Client
mqtt_client = sf_mqtt_watering_client(
    BROKER_ADDRESS, 
    BROKER_PORT, 
    BROKER_USERNAME, 
    BROKER_PASSWORD
)

# Start the MQTT background loop
mqtt_client.start_client()

# --- SCHEDULE ID COUNTER ---
# Note: IDs now come directly from hardware via send_new_schedule()

# --- HELPER: FILE PERSISTENCE ---
def load_schedules():
    """Load schedules from JSON file"""
    if os.path.exists(SCHEDULES_FILE):
        try:
            with open(SCHEDULES_FILE, 'r') as f:
                data = json.load(f)
                return data.get('schedules', [])
        except Exception as e:
            print(f"Error loading schedules: {e}")
            return []
    return []

def save_schedules(schedules):
    """Save schedules to JSON file"""
    try:
        with open(SCHEDULES_FILE, 'w') as f:
            json.dump({'schedules': schedules}, f, indent=2)
        print(f"Schedules saved to {SCHEDULES_FILE}")
    except Exception as e:
        print(f"Error saving schedules: {e}")

# --- HELPER: CRON CONVERSION ---
def convert_to_cron(time_str, duration_minutes, days_array):
    day_map = {
        'SUN': '0', 'MON': '1', 'TUE': '2', 'WED': '3', 
        'THU': '4', 'FRI': '5', 'SAT': '6'
    }
    cron_days = ",".join([day_map[d] for d in days_array]) if days_array else "*"
    start_h, start_m = map(int, time_str.split(':'))
    total_start_minutes = start_h * 60 + start_m
    total_stop_minutes = (total_start_minutes + duration_minutes) % (24 * 60)
    stop_h = total_stop_minutes // 60
    stop_m = total_stop_minutes % 60
    start_cron = f"* {start_m} {start_h} * * {cron_days}"
    stop_cron = f"* {stop_m} {stop_h} * * {cron_days}"
    return start_cron, stop_cron

# --- API ENDPOINT: GET ALL SCHEDULES ---
@app.route('/api/schedules', methods=['GET'])
def get_schedules():
    """Retrieve all saved schedules"""
    try:
        schedules = load_schedules()
        return jsonify({
            "status": "success",
            "schedules": schedules
        })
    except Exception as e:
        print(f"Error retrieving schedules: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

# --- API ENDPOINT: CREATE SCHEDULE ---
@app.route('/api/schedule', methods=['POST'])
def create_schedule():
    data = request.json
    print(f"Received Schedule Request: {data}")
    try:
        days = data.get('days', [])
        time_str = data.get('time', '00:00')
        duration = int(data.get('duration', 0))
        area = data.get('area', 'General')
        start_cron, stop_cron = convert_to_cron(time_str, duration, days)
        print(f" sending to HW -> Start: '{start_cron}', Stop: '{stop_cron}', Area: '{area}'")
        hw_id = mqtt_client.send_new_schedule(start_cron, stop_cron, area)
        
        # Check if hardware returned an error (-1)
        if hw_id == -1:
            return jsonify({
                "status": "error",
                "message": "Hardware failed to create schedule"
            }), 500
        
        # Create schedule object with hardware ID
        schedule = {
            "id": hw_id,
            "days": days,
            "time": time_str,
            "duration": duration,
            "area": area,
            "start_cron": start_cron,
            "stop_cron": stop_cron
        }
        
        # Save to file
        schedules = load_schedules()
        schedules.append(schedule)
        save_schedules(schedules)
        
        print(f"Schedule saved with HW ID: {hw_id}")
        return jsonify({
            "status": "success",
            "message": "Schedule sent to hardware and saved",
            **schedule
        })
    except Exception as e:
        print(f"Error processing schedule: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

# --- API ENDPOINT: DELETE SCHEDULE ---
@app.route('/api/schedule/<int:schedule_id>', methods=['DELETE'])
def delete_schedule(schedule_id):
    print(f"Received Schedule Delete Request for ID: {schedule_id}")
    try:
        result = mqtt_client.remove_schedule(schedule_id)
        
        # Check if hardware returned an error (-1)
        if result == -1:
            return jsonify({
                "status": "error",
                "message": "Hardware failed to remove schedule"
            }), 500
        
        # Remove from file
        schedules = load_schedules()
        schedules = [s for s in schedules if s['id'] != schedule_id]
        save_schedules(schedules)
        
        return jsonify({
            "status": "success",
            "message": "Schedule removed from hardware and saved",
            "id": schedule_id
        })
    except Exception as e:
        print(f"Error deleting schedule: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

if __name__ == '__main__':
    app.run(port=5000, debug=True)
