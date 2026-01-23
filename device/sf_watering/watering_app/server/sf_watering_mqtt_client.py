"""
@file sf_watering_mqtt_client.py
@brief MQTT client for SimFlow watering system.
"""
import random
import struct
import time
import logging
import threading
from paho.mqtt import client as mqtt_client

FIRST_RECONNECT_DELAY = 1
RECONNECT_RATE = 2
MAX_RECONNECT_COUNT = 12
MAX_RECONNECT_DELAY = 60
MIN_SF_WATERING_RESPONS = 9
SF_HW_TIMEOUT = 15


SF_WATERING_ADD_SCHEDULE    = 1
SF_WATERING_REMOVE_SCHEDULE = 2
SF_WATERING_GET_SCHEDULES   = 3


class sf_mqtt_watering_client:
    _broker_address:str
    _broker_port:str
    _broker_username:str
    _broker_pass:str
    _client: mqtt_client.Client
    _topic_watering_schedule = "sf_watering/schedule"
    _topic_watering_status = "sf_watering/watering_status"

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    def _on_message(self, client, userdata, msg):
        my_buffer = bytearray()
        my_buffer.extend(msg.payload)
        print(f"Received msg from `{msg.topic}` topic")
        
        # Extract schedule ID from response (bytes 1-4 contain the ID)
        if len(my_buffer) >= MIN_SF_WATERING_RESPONS:
            try:
                hw_res = struct.unpack('<I', my_buffer[5:9])[0]
                self._last_response_data =  my_buffer[9:]
                self._last_response = hw_res
                self._response_event.set()  # Signal that response arrived
                print(f"Extracted HW response: \nres: {hw_res} \ndata: {self._last_response_data}")
            except Exception as e:
                print(f"Error parsing response: {e}")
    
    @staticmethod
    def _on_disconnect(client, userdata, rc):
        logging.info("Disconnected with result code: %s", rc)
        reconnect_count, reconnect_delay = 0, FIRST_RECONNECT_DELAY
        while reconnect_count < MAX_RECONNECT_COUNT:
            logging.info("Reconnecting in %d seconds...", reconnect_delay)
            time.sleep(reconnect_delay)
            try:
                client.reconnect()
                logging.info("Reconnected successfully!")
                return
            except Exception as err:
                logging.error("%s. Reconnect failed. Retrying...", err)
            reconnect_delay *= RECONNECT_RATE
            reconnect_delay = min(reconnect_delay, MAX_RECONNECT_DELAY)
            reconnect_count += 1
        logging.info("Reconnect failed after %s attempts. Exiting...", reconnect_count)

    def _publish_msg(self, msg:bytearray):
        result = self._client.publish(self._topic_watering_schedule, msg)
        status = result[0]
        if status == 0:
            print(f"Send `{msg}` to topic `{self._topic_watering_schedule}`")
        else:
            print(f"Failed to send message to topic {self._topic_watering_schedule}")
            
    def __init__(self, broker_addr, broker_port, broker_usrname, broker_pass):
        self._broker_address = broker_addr
        self._broker_port = broker_port
        self._broker_username = broker_usrname
        self._broker_pass = broker_pass
        
        # Response tracking for async operations
        self._response_event = threading.Event()
        self._last_response = None
        self._last_response_data = None
        
        client_id = f'publish-{random.randint(0, 1000)}'
        self._client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION1, client_id)
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.user_data_set(self)  # Pass self as userdata
        self._client.connect(self._broker_address, self._broker_port)
        self._client.on_message = self._on_message
        self._client.subscribe(self._topic_watering_status)


    def start_client(self):
        self._client.loop_start()

    def send_new_schedule(self, start_time:str, stop_time:str, area:str):
        start_exp_len = len(start_time) + 1
        stop_exp_len = len(stop_time) + 1
        area_len = len(area) + 1
        my_buffer = bytearray()
        my_buffer.append(SF_WATERING_ADD_SCHEDULE)
        my_buffer.extend(struct.pack('<I',start_exp_len + stop_exp_len + area_len + 3))
        my_buffer.extend(struct.pack('<B',start_exp_len))
        my_buffer.extend(start_time.encode("utf-8"))
        my_buffer.append(0)
        my_buffer.extend(struct.pack('<B',stop_exp_len))
        my_buffer.extend(stop_time.encode("utf-8"))
        my_buffer.append(0)
        my_buffer.extend(struct.pack('<B',area_len))
        my_buffer.extend(area.encode("utf-8"))
        my_buffer.append(0)
        
        # Clear previous response and publish
        self._response_event.clear()
        self._last_response = None
        self._publish_msg(my_buffer)
        
        # Wait for hardware response 
        if self._response_event.wait(timeout=SF_HW_TIMEOUT):
            return self._last_response, self._last_response_data
        else:
            print("Timeout waiting for response from hardware")
            return -1, None

    def get_schedules(self):
        my_buffer = bytearray()
        my_buffer.append(SF_WATERING_GET_SCHEDULES)
        my_buffer.extend(struct.pack('<I',1))
        self._response_event.clear()
        self._last_response = None
        self._publish_msg(my_buffer)

         # Wait for hardware response 
        if self._response_event.wait(timeout=SF_HW_TIMEOUT):
            return self._last_response, self._last_response_data
        else:
            print("Timeout waiting for response from hardware")
            return -1, None

    def remove_schedule(self, schedule_id:int):
        my_buffer = bytearray()
        my_buffer.append(SF_WATERING_REMOVE_SCHEDULE)
        my_buffer.extend(struct.pack('<I',4)) 
        my_buffer.extend(struct.pack('<I',schedule_id))
        
        # Clear previous response and publish
        self._response_event.clear()
        self._last_response = None
        self._publish_msg(my_buffer)
        
        # Wait for hardware response 
        if self._response_event.wait(timeout=SF_HW_TIMEOUT):
            return self._last_response, self._last_response_data
        else:
            print("Timeout waiting for response from hardware")
            return -1, None


    def unsubscribe(self):
        return
