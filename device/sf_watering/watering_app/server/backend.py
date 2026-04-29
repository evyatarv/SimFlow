import time
from flask import Flask, request, jsonify
from flask_cors import CORS
from sf_watering_mqtt_client import sf_mqtt_watering_client

app = Flask(__name__)
CORS(app)

# --- CONFIGURATION ---
BROKER_ADDRESS  = "localhost"
BROKER_PORT     = 1883
BROKER_USERNAME = "user"
BROKER_PASSWORD = "password"

# Initialize MQTT client
mqtt_client = sf_mqtt_watering_client(
    BROKER_ADDRESS,
    BROKER_PORT,
    BROKER_USERNAME,
    BROKER_PASSWORD
)
mqtt_client.start_client()

# --- IN-MEMORY SCHEDULE STORE ---
# Populated from device on startup; kept in sync via POST/DELETE.
# Fields per entry: id, area, start_cron, stop_cron
_schedules = []

def _fetch_schedules_from_device():
    global _schedules
    num, schedules = mqtt_client.get_schedules()
    if num < 0:
        print("Warning: failed to fetch schedules from device on startup — starting with empty list")
        return
    _schedules = schedules
    print(f"Loaded {len(_schedules)} schedule(s) from device")

time.sleep(1)  # allow MQTT connection to establish
_fetch_schedules_from_device()

# --- HELPER: CRON CONVERSION ---
def convert_to_cron(time_str, duration_minutes, days_array):
    day_map = {
        'SUN': '0', 'MON': '1', 'TUE': '2', 'WED': '3',
        'THU': '4', 'FRI': '5', 'SAT': '6'
    }
    cron_days = ",".join([day_map[d] for d in days_array]) if days_array else "*"
    start_h, start_m = map(int, time_str.split(':'))
    total_start_minutes = start_h * 60 + start_m
    total_stop_minutes  = (total_start_minutes + duration_minutes) % (24 * 60)
    stop_h = total_stop_minutes // 60
    stop_m = total_stop_minutes % 60
    start_cron = f"0 {start_m} {start_h} * * {cron_days}"
    stop_cron = f"0 {stop_m} {stop_h} * * {cron_days}"
    return start_cron, stop_cron

# --- API ENDPOINT: GET ALL SCHEDULES ---
@app.route('/api/schedules', methods=['GET'])
def get_schedules():
    return jsonify({"status": "success", "schedules": _schedules})

# --- API ENDPOINT: CREATE SCHEDULE ---
@app.route('/api/schedule', methods=['POST'])
def create_schedule():
    global _schedules
    data = request.json
    print(f"Received Schedule Request: {data}")
    try:
        days       = data.get('days', [])
        time_str   = data.get('time', '00:00')
        duration   = int(data.get('duration', 0))
        area       = data.get('area', 'General')
        start_cron, stop_cron = convert_to_cron(time_str, duration, days)

        schedule_id = max((s['id'] for s in _schedules), default=-1) + 1

        print(f"Sending to HW -> ID: {schedule_id}, Start: '{start_cron}', Stop: '{stop_cron}', Area: '{area}'")
        hw_status, _ = mqtt_client.send_new_schedule(schedule_id, start_cron, stop_cron, area)

        if hw_status != 0:
            return jsonify({"status": "error", "message": "Hardware failed to create schedule"}), 500

        schedule = {
            "id":         schedule_id,
            "area":       area,
            "start_cron": start_cron,
            "stop_cron":  stop_cron,
        }
        _schedules.append(schedule)

        print(f"Schedule created with ID: {schedule_id}")
        return jsonify({"status": "success", "message": "Schedule sent to hardware", **schedule})
    except Exception as e:
        print(f"Error processing schedule: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

# --- API ENDPOINT: DELETE SCHEDULE ---
@app.route('/api/schedule/<int:schedule_id>', methods=['DELETE'])
def delete_schedule(schedule_id):
    global _schedules
    print(f"Received Schedule Delete Request for ID: {schedule_id}")
    try:
        hw_status, _ = mqtt_client.remove_schedule(schedule_id)

        if hw_status != 0:
            return jsonify({"status": "error", "message": "Hardware failed to remove schedule"}), 500

        _schedules = [s for s in _schedules if s['id'] != schedule_id]

        return jsonify({"status": "success", "message": "Schedule removed from hardware", "id": schedule_id})
    except Exception as e:
        print(f"Error deleting schedule: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

if __name__ == '__main__':
    app.run(port=5000, debug=True)
