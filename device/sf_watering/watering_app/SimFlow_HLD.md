# SimFlow Watering App - High-Level Design (HLD)

## 1. Executive Summary
**SimFlow Watering** is a web-based frontend application designed to manage and monitor automated irrigation systems. It provides an intuitive interface for users to schedule watering cycles across different zones (areas), view historical usage data, and monitor real-time system status.

## 2. System Architecture

### 2.1 Technology Stack
* **Frontend:** React, Tailwind CSS, `lucide-react` icons.
* **Backend Middleware:** Python (Flask), `paho-mqtt`.
* **Communication:** HTTP (Frontend <-> Middleware) and MQTT (Middleware <-> Hardware).
* **Data Flow:** User Input (JSON) -> Middleware -> Cron Conversion -> Binary Packing -> MQTT Broker -> Device.

### 2.2 Architecture Diagram
```mermaid
graph TD
    User[User Interface (React)] -- HTTP POST (JSON) --> Middleware[Python Flask Server]
    
    subgraph Middleware Logic
        Parse[Parse JSON]
        Convert[Convert Time to Cron]
        Pack[Binary Pack Data]
    end
    
    Middleware -- Call Method --> MQTT_Client[sf_mqtt_watering_client]
    MQTT_Client -- MQTT Publish (Binary) --> Broker[MQTT Broker]
    Broker -- Subscribe --> Device[Watering Hardware]
```

## 3. Code Organization & Documentation

### 3.1 Frontend (`App.jsx`)
The React code follows strict annotation standards:
* **`// [LABEL]`**: UI text strings (e.g., "Save Schedule").
* **`// [ICON]`**: Icon imports and definitions.
* **`// [COMPONENT]`**: Major UI blocks (Header, Sidebar).
* **`// [LOGIC]`**: Business logic (timers, API calls).

### 3.2 Backend (`backend.py`)
The Python middleware handles the translation layer:
* **API Endpoint:** `/api/schedule` (POST)
* **Helper:** `convert_to_cron()` transforms "07:00" + "30 min" into standard cron strings.
* **Integration:** Imports and uses `sf_mqtt_watering_client` to communicate with the broker.
