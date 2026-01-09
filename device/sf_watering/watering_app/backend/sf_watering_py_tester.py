from sf_watering_mqtt_client import sf_mqtt_watering_client


def print_title():
    """Print ASCII art title 'SF Watering Tester' (18 lines high with 3D shadow)"""
    title = [
"**********************************************************************************************",
"*    ███████╗███████╗    ██╗    ██╗ █████╗ ████████╗███████╗██████╗ ██╗███╗   ██╗ ██████╗    *",
"*    ██╔════╝██╔════╝    ██║    ██║██╔══██╗╚══██╔══╝██╔════╝██╔══██╗██║████╗  ██║██╔════╝    *",
"*    ███████╗█████╗      ██║ █╗ ██║███████║   ██║   █████╗  ██████╔╝██║██╔██╗ ██║██║  ███╗   *",
"*    ╚════██║██╔══╝      ██║███╗██║██╔══██║   ██║   ██╔══╝  ██╔══██╗██║██║╚██╗██║██║   ██║   *",
"*    ███████║██║         ╚███╔███╔╝██║  ██║   ██║   ███████╗██║  ██║██║██║ ╚████║╚██████╔╝   *",
"*    ╚══════╝╚═╝          ╚══╝╚══╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝    *",
"*                                                                                            *",
"*         █████╗ ██████╗ ██╗    ████████╗███████╗███████╗████████╗███████╗██████╗            *",
"*        ██╔══██╗██╔══██╗██║    ╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝██╔════╝██╔══██╗           *",
"*        ███████║██████╔╝██║       ██║   █████╗  ███████╗   ██║   █████╗  ██████╔╝           *",
"*        ██╔══██║██╔═══╝ ██║       ██║   ██╔══╝  ╚════██║   ██║   ██╔══╝  ██╔══██╗           *",
"*        ██║  ██║██║     ██║       ██║   ███████╗███████║   ██║   ███████╗██║  ██║           *",
"*        ╚═╝  ╚═╝╚═╝     ╚═╝       ╚═╝   ╚══════╝╚══════╝   ╚═╝   ╚══════╝╚═╝  ╚═╝           *",
"**********************************************************************************************",
    ]
    for line in title:
        print(line)



# --- CONFIGURATION ---
BROKER_ADDRESS = "localhost" 
BROKER_PORT = 1883
BROKER_USERNAME = "user"    
BROKER_PASSWORD = "password" 
TOPIC = "sf_watering/schedule"

# Initialize MQTT Client
mqtt_client = sf_mqtt_watering_client(
    BROKER_ADDRESS, 
    BROKER_PORT, 
    BROKER_USERNAME, 
    BROKER_PASSWORD, 
    TOPIC
)


def pre_run():
    print_title()

    # Start the MQTT background loop
    mqtt_client.start_client()

    

def post_run():
    pass



def test_get_schedules():
    mqtt_client.send_new_schedule("* 23 17 * * *", "* 23 18 * * *", "Test Area")
    mqtt_client.get_schedules()



def main():
    pre_run(); 

    #tests 
    test_get_schedules()


    post_run()

if __name__ == "__main__":
    main()