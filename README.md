# ControlRobot - Sistema de Control Remoto para Robots con ESP32 y Servidor Orion

**ControlRobot** es un proyecto basado en **PlatformIO** y **Python (Flask)** que permite el control remoto de un robot mediante comunicación Wi-Fi entre un **ESP32** y un **servidor Orion**, con una interfaz web en tiempo real.

---

##  Estructura del Proyecto
ControlRobot/
├── platformio.ini
├── src/
│ ├── main.cpp
│ ├── control.h
│ ├── control.cpp
│ └── wifi_config.h
├── include/
│ └── defines.h
├── orionserver/
│ ├── orionserver/
│ │ ├── server.py
│ │ ├── Dockerfile
│ │ └── requirements.txt
└── README.md


## Tecnologías Utilizadas

| Componente | Tecnología / Librería | Descripción |
|-------------|----------------------|--------------|
| Servidor web | **Flask** | Framework principal para manejar peticiones HTTP. |
| WSGI Server | **Gunicorn** | Servidor de producción para Flask. |
| Despliegue | **AWS EC2** | Máquina virtual donde se aloja el servidor. |
| Comunicación inalámbrica | **LoRa (SX1276)** | Enlace de baja frecuencia y largo alcance entre TX y RX. |
| Hardware TX | **ESP32 (LilyGO T-Beam)** | Microcontrolador que transmite comandos LoRa. |
| Hardware RX | **ESP32 (LilyGO T-Beam) + L298N** | Controla motores izquierdo y derecho. |
| Lenguaje backend | **Python 3** | Lógica del servidor. |
| Serialización | **ArduinoJson** | Para codificar/decodificar mensajes JSON en el TX/RX. |


