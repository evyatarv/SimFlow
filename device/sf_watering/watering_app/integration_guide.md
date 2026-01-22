# SimFlow Project Integration Guide

This guide explains how to assemble the React Web UI and Python Server into a working IoT application.

## 1. Directory Structure

Organize your project folder as follows:

```text
watering_app/
├── server/                        # Python Middleware
│   ├── backend.py                 # The Flask API logic
│   ├── sf_watering_mqtt_client.py # The MQTT Client Class
│   └── requirements.txt           # Python dependencies
│
├── webui/                         # React Application
│   ├── public/
│   │   └── SIM_FLOW_LOGO.jpg      # Your logo file
│   ├── src/
│   │   ├── App.jsx                # The React UI Component
│   │   ├── App.test.jsx           # Unit tests
│   │   ├── index.css              # Tailwind directives
│   │   └── test/
│   │       └── setup.js           # Test setup
│   ├── package.json
│   ├── vite.config.js
│   └── tailwind.config.js
│
├── Makefile                       # Build automation
├── integration_guide.md           # This file
└── SimFlow_HLD.md                 # High-level design
```

## 2. Quick Start with Makefile

The project includes a Makefile for common development tasks. Run these commands from the project root:

### Web UI Commands

| Command | Description |
|---------|-------------|
| `make webui-dev` | Start the development server |
| `make webui-build` | Create production build |
| `make webui-test` | Run unit tests once |
| `make webui-test-watch` | Run tests in watch mode |
| `make webui-lint` | Run ESLint |

### Server Commands

| Command | Description |
|---------|-------------|
| `make server-install` | Install Python dependencies |
| `make server-run` | Start the Flask server |

### Example Workflow

```bash
# First time setup
make server-install

# Start both services (in separate terminals)
make server-run      # Terminal 1
make webui-dev       # Terminal 2

# Run tests before committing
make webui-test
make webui-lint
```

## 3. Server Setup (Python)

1.  **Navigate to the server folder:** `cd server`
2.  **Create a virtual environment:** `python -m venv venv`
3.  **Activate the environment:**
    - Linux/Mac: `source venv/bin/activate`
    - Windows: `venv\Scripts\activate`
4.  **Install Dependencies:** `pip install -r requirements.txt`
5.  **Run the Server:** `python backend.py`

Or use the Makefile:
```bash
make server-install
make server-run
```

## 4. Web UI Setup (React + Vite)

1.  **Navigate to the webui folder:** `cd webui`
2.  **Install Dependencies:** `npm install`
3.  **Run the UI:** `npm run dev`

Or use the Makefile:
```bash
make webui-dev
```

## 5. Testing

The Web UI includes unit tests using Vitest and React Testing Library.

```bash
# Run tests once
make webui-test

# Run tests in watch mode (re-runs on file changes)
make webui-test-watch
```
