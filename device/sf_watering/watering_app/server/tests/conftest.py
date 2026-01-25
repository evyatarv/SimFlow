"""
@file conftest.py
@brief Pytest configuration and shared fixtures for SF Watering tests.
"""
import sys
from pathlib import Path
import pytest
from unittest.mock import patch

# Add parent directory to path so we can import backend modules
sys.path.insert(0, str(Path(__file__).parent.parent))

from sf_watering_mqtt_client import sf_mqtt_watering_client


def print_title():
    """Print ASCII art title for test suite"""
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


def pytest_configure(config):
    """Runs once before all tests start"""
    print("\n")
    print_title()
    print("\n")


# --- CONFIGURATION ---
BROKER_ADDRESS = "localhost"
BROKER_PORT = 1883
BROKER_USERNAME = "user"
BROKER_PASSWORD = "password"


@pytest.fixture
def mqtt_client():
    """
    Fixture to provide a fresh MQTT client for each test.
    Handles setup and cleanup.
    """
    client = sf_mqtt_watering_client(
        BROKER_ADDRESS,
        BROKER_PORT,
        BROKER_USERNAME,
        BROKER_PASSWORD,
    )
    yield client
    # Cleanup
    try:
        client.stop_client()
    except Exception:
        pass


@pytest.fixture
def mock_mqtt_client(mocker):
    """
    Fixture providing a mocked MQTT client for unit tests
    without requiring actual MQTT broker connection.
    """
    with patch('sf_watering_mqtt_client.mqtt_client.Client') as mock_client:
        client = sf_mqtt_watering_client(
            BROKER_ADDRESS,
            BROKER_PORT,
            BROKER_USERNAME,
            BROKER_PASSWORD,
        )
        yield client, mock_client
