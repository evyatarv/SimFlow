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

        res, data = mqtt_client.send_new_schedule("* 23 16 * * *", "* 8 18 * * *", "Test Area1")
        time.sleep(1)
        # Add assertions based on your expected behavior
        assert res > -1
    def test_get_schedules(self, mqtt_client):
        """Test retrieving schedules"""
        mqtt_client.start_client()
        time.sleep(1)

        res, data = mqtt_client.get_schedules()
        time.sleep(1)
        # Add assertions based on response
        assert res > -1

    def test_remove_schedule(self, mqtt_client):
        """Test removing a schedule"""
        mqtt_client.start_client()
        time.sleep(1)

        hw_id, data = mqtt_client.send_new_schedule("* 23 16 * * *", "* 8 18 * * *", "Test Area1")
        time.sleep(1)
        assert hw_id > -1

        num_of_chedules_before, data = mqtt_client.get_schedules()
        time.sleep(1)
        assert num_of_chedules_before > -1

        res, data = mqtt_client.remove_schedule(hw_id)
        time.sleep(1)
        assert res > -1

        num_of_chedules_after, data = mqtt_client.get_schedules()
        time.sleep(1)
        assert num_of_chedules_after > -1

        assert num_of_chedules_after + 1 == num_of_chedules_before



    def test_multiple_schedules(self, mqtt_client):
        """Test sending multiple schedules"""
        
        mqtt_client.start_client()
        time.sleep(1)
        
        num_of_chedules_before, data = mqtt_client.get_schedules()
        time.sleep(1)
        assert num_of_chedules_before > -1

        hw_id, data = mqtt_client.send_new_schedule("* 23 17 * * *", "* 23 18 * * *", "Test Area2")
        time.sleep(1)
        assert hw_id > -1

        hw_id, data = mqtt_client.send_new_schedule("* 23 17 * * *", "* 23 18 * * *", "Test Area2")
        time.sleep(1)
        assert hw_id > -1

        hw_id, data = mqtt_client.send_new_schedule("* 23 17 * * *", "* 23 18 * * *", "Test Area2")
        time.sleep(1)
        assert hw_id > -1

        hw_id, data = mqtt_client.send_new_schedule("* 23 17 * * *", "* 23 18 * * *", "Test Area2")
        time.sleep(1)
        assert hw_id > -1

        hw_id, data = mqtt_client.send_new_schedule("* 23 17 * * *", "* 23 18 * * *", "Test Area2")
        time.sleep(1)
        assert hw_id > -1

        num_of_chedules_after, data = mqtt_client.get_schedules()
        time.sleep(1)
        assert num_of_chedules_after > -1

        assert num_of_chedules_after - 5 == num_of_chedules_before


class TestMockedOperations:
    """Test suite using mocked MQTT client (no broker required)"""


# --- INTEGRATION TEST (requires actual MQTT broker) ---

@pytest.mark.integration
class TestIntegration:
    """Integration tests that require actual MQTT broker connection"""
