import time
from sf_watering_mqtt_client import sf_mqtt_watering_client


TITLE_BANNER = """
**********************************************************************************************
*    ███████╗███████╗    ██╗    ██╗ █████╗ ████████╗███████╗██████╗ ██╗███╗   ██╗ ██████╗    *
*    ██╔════╝██╔════╝    ██║    ██║██╔══██╗╚══██╔══╝██╔════╝██╔══██╗██║████╗  ██║██╔════╝    *
*    ███████╗█████╗      ██║ █╗ ██║███████║   ██║   █████╗  ██████╔╝██║██╔██╗ ██║██║  ███╗   *
*    ╚════██║██╔══╝      ██║███╗██║██╔══██║   ██║   ██╔══╝  ██╔══██╗██║██║╚██╗██║██║   ██║   *
*    ███████║██║         ╚███╔███╔╝██║  ██║   ██║   ███████╗██║  ██║██║██║ ╚████║╚██████╔╝   *
*    ╚══════╝╚═╝          ╚══╝╚══╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝    *
*                                                                                            *
*         █████╗ ██████╗ ██╗    ████████╗███████╗███████╗████████╗███████╗██████╗            *
*        ██╔══██╗██╔══██╗██║    ╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝██╔════╝██╔══██╗           *
*        ███████║██████╔╝██║       ██║   █████╗  ███████╗   ██║   █████╗  ██████╔╝           *
*        ██╔══██║██╔═══╝ ██║       ██║   ██╔══╝  ╚════██║   ██║   ██╔══╝  ██╔══██╗           *
*        ██║  ██║██║     ██║       ██║   ███████╗███████║   ██║   ███████╗██║  ██║           *
*        ╚═╝  ╚═╝╚═╝     ╚═╝       ╚═╝   ╚══════╝╚══════╝   ╚═╝   ╚══════╝╚═╝  ╚═╝           *
**********************************************************************************************
"""


def print_title():
    print(TITLE_BANNER)



# --- CONFIGURATION ---
BROKER_ADDRESS = "localhost" 
BROKER_PORT = 1883
BROKER_USERNAME = "user"    
BROKER_PASSWORD = "password" 

# Initialize MQTT Client
mqtt_client = sf_mqtt_watering_client(
    BROKER_ADDRESS, 
    BROKER_PORT, 
    BROKER_USERNAME, 
    BROKER_PASSWORD, 
)

def pre_run():
    print_title()

    # Start the MQTT background loop
    mqtt_client.start_client()

    

def post_run():
    pass



def test_get_schedules():
    mqtt_client.send_new_schedule("* 23 16 * * *", "* 23 18 * * *", "Test Area1")
    time.sleep(1)
    mqtt_client.send_new_schedule("* 23 17 * * *", "* 23 18 * * *", "Test Area2")
    time.sleep(1)
    mqtt_client.send_new_schedule("* 23 18 * * *", "* 23 18 * * *", "Test Area3")
    time.sleep(1)
    mqtt_client.get_schedules()
    time.sleep(1)
    mqtt_client.remove_schedule(1)
    time.sleep(1)
    mqtt_client.remove_schedule(2)
    time.sleep(1)
    mqtt_client.get_schedules()
    time.sleep(10)

def main():
    pre_run()

    #tests 
    test_get_schedules()


    post_run()

if __name__ == "__main__":
    main()