# Pytest commands cheat sheet for SF Watering tests

## Installation
# Install test dependencies in addition to main requirements:
pip install -r requirements.txt -r requirements-test.txt

## Running Tests

# Run all tests
pytest

# Run only unit tests (no broker required)
pytest -m "not integration"

# Run only integration tests (requires MQTT broker)
pytest -m integration

# Run with verbose output
pytest -v

# Run specific test file
pytest sf_watering_py_tester.py

# Run specific test class
pytest sf_watering_py_tester.py::TestScheduleOperations

# Run specific test function
pytest sf_watering_py_tester.py::TestScheduleOperations::test_send_new_schedule

## Coverage Reports

# Generate coverage report
pytest --cov=. --cov-report=html

# View coverage (opens htmlcov/index.html in browser)
start htmlcov/index.html

## Useful Flags

# Stop on first failure
pytest -x

# Show local variables in tracebacks
pytest -l

# Run with detailed output
pytest -vv

# Show print statements
pytest -s

# Run tests in parallel (requires pytest-xdist)
pytest -n auto

## Example Workflows

# Development: Quick unit tests with output
pytest -m "not integration" -s

# Full test suite with coverage
pytest --cov=. --cov-report=html

# CI/CD: Tests + coverage report
pytest -v --cov=. --cov-report=xml
