<p align="center">
  <img src="logos/SIM_FLOW_LOGO.png" alt="SimFlow Logo" width="150"/>
</p>

<h1 align="center">SimFlow: A Modular Framework for Simple IoT</h1>

<p align="center">
  <strong>Build simple, automated devices with reusable embedded components.</strong>
  <br />
  SimFlow is an open-source project based on ESP-IDF that provides a modular foundation for creating custom IoT solutions.
</p>

---

## The SimFlow Philosophy

The goal of SimFlow is to accelerate the development of simple, single-purpose embedded devices. Instead of starting from scratch, you can leverage a set of robust, pre-built firmware **components** (like GPIO controllers, WiFi managers, and timers) to bring your ideas to life quickly.

### Core Components

The power of SimFlow lies in its modular firmware architecture. The core components provide abstractions for common hardware and connectivity tasks:

-   **GPIO Control:** Simple interfaces for digital input/output.
-   **Timers & Scheduling:** Reliable timing and event scheduling.
-   **WiFi Manager:** Handles network connectivity and credentials.
-   **MQTT Client:** Manages communication with a broker.
-   *(and more...)*

By combining these building blocks, you can create a new device with minimal boilerplate code.

## Example Application: The `sf_watering` Smart Irrigator

To demonstrate the power of the SimFlow framework, we created the **Smart Watering App**. It uses the core SimFlow components to control a water valve and provides a beautiful web interface for scheduling and monitoring.

*Note: Add a screenshot or GIF of your application here!*

### How to Run the Watering App

**1. Run the Frontend:**
```bash
# The frontend is in the /frontend directory
cd frontend
npm install
npm run dev
```
The React app will be available at `http://localhost:5173`.

**2. Run the Backend Middleware:**
```bash
cd device/sf_watering/watering_app/backend
pip install -r requirements.txt
flask run
```
The Flask server will run on `http://localhost:5000`.

**3. Run the Firmware:**    

For starting guide for ESP-IDF toolchain, flash and montoring HW see [ESD-IDF Starting Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).  

The SimFlow devices firmwareis located in `./device/`.  
For example: sf_watering device will loacted in `./device/sf_watering/firmware`.  
  
   
  
### Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue.

### License

This project is licensed under the MIT License.
