import json
import time
import random
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
TOPIC = "sf_watering/watering_status"

# Initialize MQTT Client
mqtt_client = sf_mqtt_watering_client(
    BROKER_ADDRESS, 
    BROKER_PORT, 
    BROKER_USERNAME, 
    BROKER_PASSWORD, 
    TOPIC
)

# Start the MQTT background loop
mqtt_client.start_client()

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
    start_cron = f"{start_m} {start_h} * * {cron_days}"
    stop_cron = f"{stop_m} {stop_h} * * {cron_days}"
    return start_cron, stop_cron

# --- API ENDPOINT ---
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
        mqtt_client.send_new_schedule(start_cron, stop_cron, area)
        hw_id = random.randint(1000, 9999) 
        return jsonify({
            "status": "success",
            "message": "Schedule sent to hardware",
            "id": hw_id, 
            **data 
        })
    except Exception as e:
        print(f"Error processing schedule: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

if __name__ == '__main__':
    app.run(port=5000, debug=True)
