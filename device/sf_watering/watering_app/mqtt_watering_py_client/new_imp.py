from paho.mqtt import client as mqtt_client


class sf_mqtt_watering_client:

    _broker_address:str
    _broker_port:str
    _broker_username:str
    _broker_pass:str

    def __init__(self):
        print("notihng")