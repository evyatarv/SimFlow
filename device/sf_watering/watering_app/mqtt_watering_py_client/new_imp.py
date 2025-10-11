"""
@file new_imp.py
@brief MQTT client for SimFlow watering system.

This module provides the sf_mqtt_watering_client class, which manages MQTT connections,
publishing, and subscribing for the SimFlow watering system. It handles connection
management, message publishing, reconnection logic, and sending new watering schedules
to the MQTT broker.

Classes:
    sf_mqtt_watering_client: Handles MQTT connection, publishing, subscribing, and
    sending watering schedule commands.

Dependencies:
    - paho.mqtt
    - struct
    - random
    - time
    - logging
"""
import random
import struct
import time
import logging
from paho.mqtt import client as mqtt_client

FIRST_RECONNECT_DELAY = 1
RECONNECT_RATE = 2
MAX_RECONNECT_COUNT = 12
MAX_RECONNECT_DELAY = 60

class sf_mqtt_watering_client:
    """
    MQTT client for SimFlow watering system.
    Handles connection, publishing, subscribing, and sending watering schedule commands.
    """

    _broker_address:str
    _broker_port:str
    _broker_username:str
    _broker_pass:str
    _client: mqtt_client.Client
    _topic_watering_status = "sf_watering/watering_status"

    @staticmethod
    def _on_connect(client, userdata, flags, rc):
        """
        Callback for when the client receives a CONNACK response from the server.

        Args:
            client: The client instance for this callback.
            userdata: The private user data as set in Client() or userdata_set().
            flags: Response flags sent by the broker.
            rc: The connection result.
        """
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    @staticmethod
    def _on_message(client, userdata, msg):
        """
        Callback for when a PUBLISH message is received from the server.

        Args:
            client: The client instance for this callback.
            userdata: The private user data as set in Client() or userdata_set().
            msg: An instance of MQTTMessage.
        """
        my_buffer = bytearray()
        my_buffer.extend(msg.payload)
        print(f"Received `{my_buffer}` from `{msg.topic}` topic")
    
    @staticmethod
    def _on_disconnect(client, userdata, rc):
        """
        Callback for when the client disconnects from the broker.
        Attempts to reconnect with exponential backoff.

        Args:
            client: The client instance for this callback.
            userdata: The private user data as set in Client() or userdata_set().
            rc: The disconnection result.
        """
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
        """
        Publish a message to the watering status topic.

        Args:
            msg (bytearray): The message payload to publish.
        """
        result = self._client.publish(self._topic_watering_status, msg)
        # result: [0, 1] 
        status = result[0]
        if status == 0:
            print(f"Send `{msg}` to topic `{self._topic_watering_status}`")
        else:
            print(f"Failed to send message to topic {self._topic_watering_status}")
            
    def __init__(self, broker_addr, broker_port, broker_usrname, broker_pass, topic_watering_status):
        """
        Initialize the MQTT watering client.

        Args:
            broker_addr (str): MQTT broker address.
            broker_port (str): MQTT broker port.
            broker_usrname (str): MQTT broker username.
            broker_pass (str): MQTT broker password.
            topic_watering_status (str): MQTT topic for watering status.
        """
        self._broker_address = broker_addr
        self._broker_port = broker_port
        self._broker_username = broker_usrname
        self._broker_pass = broker_pass
        self._topic_watering_status = topic_watering_status

        # Generate a Client ID with the publish prefix.
        client_id = f'publish-{random.randint(0, 1000)}'

        self._client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION1, client_id)
        # client.username_pw_set(username, password)
        
        # set connect and disconnect callbacks
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect

        # connect broker
        self._client.connect(self._broker_address, self._broker_port)
        
        # subscribe 
        self._client.subscribe(self._topic_watering_status)
        self._client.on_message = self._on_message

    def start_client(self):
        """
        Start the MQTT client network loop in a separate thread.
        """
        self._client.loop_start()

    def send_new_schedule(self, start_time:str, stop_time:str, area:str):
        """
        Send a new watering schedule to the MQTT broker.

        Args:
            start_time (str): Cron expression for the start time.
            stop_time (str): Cron expression for the stop time.
            area (str): Area name or identifier.
        """
        start_exp_len = len(start_time) + 1
        stop_exp_len = len(stop_time) + 1
        area_len = len(area) + 1
        my_buffer = bytearray()
        # add cmd type
        my_buffer.append(1)

        # add data size 
        my_buffer.extend(struct.pack('<I',start_exp_len + stop_exp_len + area_len + 3))

        # add data
        my_buffer.extend(struct.pack('<B',start_exp_len))
        my_buffer.extend(start_time.encode("utf-8"))
        my_buffer.append(0)

        my_buffer.extend(struct.pack('<B',stop_exp_len))
        my_buffer.extend(stop_time.encode("utf-8"))
        my_buffer.append(0)

        my_buffer.extend(struct.pack('<B',area_len))
        my_buffer.extend(area.encode("utf-8"))
        my_buffer.append(0)

        print(my_buffer)

        self._publish_msg(my_buffer)

    def unsubscribe(self):
        """
        Placeholder for unsubscribe functionality.
        """
        return