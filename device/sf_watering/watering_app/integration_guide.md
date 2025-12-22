# SimFlow Project Integration Guide

This guide explains how to assemble the React Frontend and Python Backend into a working IoT application.

## 1. Directory Structure

Organize your project folder as follows:

```text
SimFlow_Project/
├── backend/                       # Python Middleware
│   ├── backend.py                 # The Flask API logic
│   ├── sf_watering_mqtt_client.py # The MQTT Client Class (Corrected)
│   └── requirements.txt           # Python dependencies
│
└── frontend/                      # React Application
    ├── public/
    │   └── SIM_FLOW_LOGO.jpg      # Your logo file
    ├── src/
    │   ├── App.jsx                # The React UI Component
    │   └── index.css              # Tailwind directives
    ├── package.json
    └── tailwind.config.js
```

## 2. Backend Setup (Python)

1.  **Navigate to the backend folder:** `cd backend`
2.  **Create a virtual environment:** `python -m venv venv`
3.  **Install Dependencies:** `pip install flask flask-cors paho-mqtt`
4.  **Run the Server:** `python backend.py`

## 3. Frontend Setup (React + Vite)

1.  **Create a new Vite project:** `npm create vite@latest frontend -- --template react`
2.  **Install Libraries:**
    ```bash
    npm install
    npm install lucide-react
    npm install -D tailwindcss postcss autoprefixer
    npx tailwindcss init -p
    ```
3.  **Run the UI:** `npm run dev`
