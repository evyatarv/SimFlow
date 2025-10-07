
# python 3.11

import struct
import time
import logging
import random
from paho.mqtt import client as mqtt_client






broker = '192.168.1.226'
port = 1883
topic_new_scheduler = "/topic/schedule"
topic_watering_status = "sf_watering/watering_status"
client_id = f'python-mqtt-{random.randint(0, 1000)}'
# username = 'emqx'
# password = 'public'


FIRST_RECONNECT_DELAY = 1
RECONNECT_RATE = 2
MAX_RECONNECT_COUNT = 12
MAX_RECONNECT_DELAY = 60

def on_disconnect(client, userdata, rc):
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


def subscribe(client: mqtt_client):
    def on_message(client, userdata, msg):
        my_buffer = bytearray()
        my_buffer.extend(msg.payload)
        print(f"Received `{my_buffer}` from `{msg.topic}` topic")

    client.subscribe(topic_watering_status)
    client.on_message = on_message



# Generate a Client ID with the publish prefix.
client_id = f'publish-{random.randint(0, 1000)}'
# username = 'emqx'
# password = 'public'

def connect_mqtt():
    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION1, client_id)
    # client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.connect(broker, port)
    return client


def publish(client):
    msg_count = 1
    while True:
        time.sleep(1)
        cron_exp_start = f"0 8 5 * * *"
        cron_exp_end =  f"0 9 6 * * *"
        area = f"front yard"
        start_exp_len = len(cron_exp_start) + 1
        stop_exp_len = len(cron_exp_end) + 1
        area_len = len(area) + 1
        my_buffer = bytearray() # Creates a bytearray of 10 null bytes
        #add cmd type
        my_buffer.append(1)

        #add data size 
        # Convert the integer to 4 bytes (big-endian '>')
        # 'i' for signed integer, 'I' for unsigned integer
        # Use '<' for little-endian
        
        my_buffer.extend(struct.pack('<I',start_exp_len + stop_exp_len + area_len + 3))


        #add data
        my_buffer.extend(struct.pack('<B',start_exp_len))
        my_buffer.extend(cron_exp_start.encode("utf-8"))
        my_buffer.append(0)

        my_buffer.extend(struct.pack('<B',stop_exp_len))
        my_buffer.extend(cron_exp_end.encode("utf-8"))
        my_buffer.append(0)

        my_buffer.extend(struct.pack('<B',area_len))
        my_buffer.extend(area.encode("utf-8"))
        my_buffer.append(0)

        print(my_buffer)
       
        result = client.publish(topic_new_scheduler, my_buffer)
        # result: [0, 1] 
        status = result[0]
        if status == 0:
            print(f"Send `{my_buffer}` to topic `{topic_new_scheduler}`")
        else:
            print(f"Failed to send message to topic {topic_new_scheduler}")
        msg_count += 1
        if msg_count > 1:
            break


def run():    
    client = connect_mqtt()
    subscribe(client)
    client.loop_start()
    publish(client)
    client.loop_stop()


if __name__ == '__main__':
    run()
