"""
@file conftest.py
@brief Pytest configuration and shared fixtures for SF Watering tests.
"""
import os
import sys
import socket
import time
from pathlib import Path
import pytest
from unittest.mock import patch
from dotenv import load_dotenv

# Add parent directory to path so we can import backend modules
sys.path.insert(0, str(Path(__file__).parent.parent))

# 1. Try to load variables from a local .env file
# This file is ignored by Git and only exists on your local machine
env_path = os.path.join(os.path.dirname(__file__), '.env')
load_dotenv(dotenv_path=env_path)

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


def pytest_configure(config):
    """Runs once before all tests start"""
    print("\n" + TITLE_BANNER + "\n")


# --- CONFIGURATION ---
BROKER_ADDRESS  = os.getenv("MQTT_HOST", "localhost")
BROKER_PORT     = int(os.getenv("MQTT_PORT", 1883))
BROKER_USERNAME = "user"
BROKER_PASSWORD = "password"


@pytest.fixture(scope="session", autouse=True)
def wait_for_services():
    """Wait for MQTT broker to be reachable, then allow firmware time to connect."""
    deadline = time.time() + 60
    while time.time() < deadline:
        try:
            with socket.create_connection((BROKER_ADDRESS, BROKER_PORT), timeout=2):
                break
        except OSError:
            time.sleep(2)
    else:
        pytest.fail(f"MQTT broker at {BROKER_ADDRESS}:{BROKER_PORT} not reachable after 60s")
    time.sleep(5)  # Give firmware time to connect after broker is up


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
