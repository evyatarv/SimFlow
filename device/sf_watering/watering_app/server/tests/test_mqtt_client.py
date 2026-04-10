"""
@file test_mqtt_client.py
@brief Pytest-based test suite for SF Watering MQTT client.
"""
import time
import pytest


# --- UNIT TESTS ---

class TestScheduleOperations:
    """Test suite for MQTT schedule operations"""

    def test_send_new_schedule(self, mqtt_client):
        """Test sending a new schedule"""
        mqtt_client.start_client()
        time.sleep(1)

        schedule_id = 100
        status, _ = mqtt_client.send_new_schedule(schedule_id, "* 23 16 * * *", "* 8 18 * * *", "Test Area1")
        time.sleep(1)
        assert status == 0

    def test_get_schedules(self, mqtt_client):
        """Test retrieving schedules"""
        mqtt_client.start_client()
        time.sleep(1)

        res, data = mqtt_client.get_schedules()
        time.sleep(1)
        assert res > -1

    def test_remove_schedule(self, mqtt_client):
        """Test removing a schedule"""
        mqtt_client.start_client()
        time.sleep(1)

        schedule_id = 200
        status, _ = mqtt_client.send_new_schedule(schedule_id, "* 23 16 * * *", "* 8 18 * * *", "Test Area1")
        time.sleep(1)
        assert status == 0

        num_of_schedules_before, _ = mqtt_client.get_schedules()
        time.sleep(1)
        assert num_of_schedules_before > -1

        res, data = mqtt_client.remove_schedule(schedule_id)
        time.sleep(1)
        assert res > -1

        num_of_schedules_after, _ = mqtt_client.get_schedules()
        time.sleep(1)
        assert num_of_schedules_after > -1

        assert num_of_schedules_after + 1 == num_of_schedules_before


    def test_multiple_schedules(self, mqtt_client):
        """Test sending multiple schedules"""

        mqtt_client.start_client()
        time.sleep(1)

        num_of_schedules_before, _ = mqtt_client.get_schedules()
        time.sleep(1)
        assert num_of_schedules_before > -1

        for i, schedule_id in enumerate(range(300, 305)):
            status, _ = mqtt_client.send_new_schedule(schedule_id, "* 23 17 * * *", "* 23 18 * * *", "Test Area2")
            time.sleep(1)
            print(f"{i+1} --> schedule_id: {schedule_id}, status: {status}")
            assert status == 0

        num_of_schedules_after, _ = mqtt_client.get_schedules()
        time.sleep(1)
        print(f"--> num_of_schedules_after: {num_of_schedules_after}")
        assert num_of_schedules_after > -1

        assert num_of_schedules_after - 5 == num_of_schedules_before


class TestMockedOperations:
    """Test suite using mocked MQTT client (no broker required)"""


# --- INTEGRATION TEST (requires actual MQTT broker) ---

@pytest.mark.integration
class TestIntegration:
    """Integration tests that require actual MQTT broker connection"""
