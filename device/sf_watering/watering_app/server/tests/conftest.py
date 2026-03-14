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


# --- CONFIGURATION ---
BROKER_ADDRESS  = os.getenv("MQTT_HOST", "localhost")
BROKER_PORT     = int(os.getenv("MQTT_PORT", 1883))
BROKER_USERNAME = "user"
BROKER_PASSWORD = "password"


def pytest_configure(config):
    """Runs once before all tests start"""
    print("\n" + TITLE_BANNER + "\n")


def pytest_sessionstart():
    """Wait for MQTT broker and firmware before any tests run (outside pytest-timeout scope)."""
    # Step 1: wait for broker TCP port
    deadline = time.time() + 60
    while time.time() < deadline:
        try:
            with socket.create_connection((BROKER_ADDRESS, BROKER_PORT), timeout=2):
                break
        except OSError:
            time.sleep(1)
    else:
        pytest.exit(f"MQTT broker at {BROKER_ADDRESS}:{BROKER_PORT} not reachable after 60s", returncode=1)

    # Step 2: probe firmware with get_schedules() until it responds
    probe = sf_mqtt_watering_client(BROKER_ADDRESS, BROKER_PORT, BROKER_USERNAME, BROKER_PASSWORD)
    probe.start_client()
    time.sleep(1)

    deadline = time.time() + 90
    while time.time() < deadline:
        res, _ = probe.get_schedules()
        if res > -1:
            break
    else:
        probe.stop_client()
        pytest.exit("Firmware did not respond to MQTT within 90s", returncode=1)

    probe.stop_client()


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
