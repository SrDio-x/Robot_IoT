# Usa una imagen base oficial de Python (ligera)
FROM python:3.10-slim

# Establece el directorio de trabajo dentro del contenedor
WORKDIR /app

# Copia los archivos de requerimientos e instálalos
COPY requirements.txt .

# Instala las dependencias necesarias, incluyendo Flask, Gunicorn y gpiozero
RUN pip install --no-cache-dir -r requirements.txt gpiozero requests

# Copia el código de la aplicación al contenedor
COPY server.py .

# Expone el puerto 80 para acceder al servidor Flask
EXPOSE 80

# Comando para ejecutar la aplicación con Gunicorn
# 0.0.0.0:80 asegura que el servidor escuche en la IP pública (ideal para EC2 o Raspberry Pi)
CMD ["gunicorn", "-w", "1", "-b", "0.0.0.0:80", "server:app"]
