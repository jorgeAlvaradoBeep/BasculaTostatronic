# Báscula Digital ESP32 con HX711

## Descripción
Código completo para báscula digital con ESP32 DevKit V1, sensor HX711, LCD 16x2 I2C y encoder rotatorio KY-040. Incluye modo peso, modo conteo de piezas, calibración y conexión WiFi.

## Componentes Necesarios
- ESP32 DevKit V1
- HX711 (módulo amplificador)
- Celda de carga / Galga extensométrica
- LCD 16x2 con módulo I2C
- Encoder rotatorio KY-040
- Pesas de calibración (50g, 100g, 200g, 500g o 1000g)

## Conexiones

### HX711
- **DOUT** → GPIO 16 (ESP32)
- **SCK** → GPIO 4 (ESP32)
- **VCC** → 5V
- **GND** → GND

### LCD I2C
- **SDA** → GPIO 21 (ESP32)
- **SCL** → GPIO 22 (ESP32)
- **VCC** → 5V
- **GND** → GND

### Encoder KY-040
- **CLK** → GPIO 25 (ESP32)
- **DT** → GPIO 26 (ESP32)
- **SW** → GPIO 27 (ESP32)
- **+** → 3.3V
- **GND** → GND

## Librerías Necesarias
Instalar desde el Administrador de Librerías de Arduino IDE:

1. **HX711** (by Bogdan Necula o similar)
2. **LiquidCrystal_I2C** (by Frank de Brabander)

Las librerías WiFi y Preferences vienen incluidas con el núcleo ESP32.

## Configuración Inicial

### 1. Instalar Soporte para ESP32
En Arduino IDE:
- Ir a **Archivo → Preferencias**
- En "Gestor de URLs Adicionales de Tarjetas" agregar:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```
- Ir a **Herramientas → Placa → Gestor de Tarjetas**
- Buscar "ESP32" e instalar "esp32 by Espressif Systems"

### 2. Configurar WiFi
En el código, modificar las líneas 24-25:
```cpp
const char* ssid = "TU_RED_WIFI";          // Nombre de tu red WiFi
const char* password = "TU_CONTRASEÑA";    // Contraseña de tu red
```

### 3. Verificar Dirección I2C del LCD
La dirección común es **0x27**, pero algunos módulos usan **0x3F**.
Para verificar, usar este sketch de escaneo I2C:
```cpp
#include <Wire.h>
void setup() {
  Wire.begin();
  Serial.begin(115200);
  Serial.println("\nEscaneando I2C...");
  for(byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Dispositivo en: 0x");
      Serial.println(i, HEX);
    }
  }
}
void loop() {}
```
Si encuentra **0x3F**, cambiar en línea 30:
```cpp
LiquidCrystal_I2C lcd(0x3F, 16, 2);
```

## Uso del Sistema

### Primer Inicio
1. Al encender por primera vez, el sistema solicitará calibración
2. Seguir las instrucciones en pantalla
3. Tener listas pesas de calibración conocidas

### Modo Peso (Báscula Normal)
- Muestra el peso actual en gramos
- Actualización continua

### Funciones del Encoder

#### Presión Corta (< 3 segundos)
- En menús: Selecciona la opción mostrada

#### Presión 3 Segundos
- **En Modo Peso**: Abre menú principal
  - Calibración
  - Red (ver IP)
  - Reiniciar (borrar EEPROM)
- **En Modo Piezas**: Abre menú de calibración de piezas

#### Presión 5 Segundos
- **En Modo Peso**: Cambia a Modo Piezas
- **En Modo Piezas**: Regresa a Modo Peso

#### Girar Encoder
- En menús: Navega entre opciones

### Calibración de Peso

1. Desde el menú principal, seleccionar "Calibracion"
2. Girar encoder para seleccionar peso conocido (50g, 100g, 200g, 500g, 1000g)
3. Presionar encoder para confirmar
4. **Retirar todo peso** de la báscula
5. Esperar conteo regresivo (5 segundos)
6. **Colocar peso seleccionado** en la báscula
7. Esperar conteo regresivo (5 segundos)
8. ¡Calibración completa!

### Modo Conteo de Piezas

#### Activar Modo
- Mantener presionado el encoder durante **5 segundos** desde Modo Peso

#### Calibrar Piezas
1. En Modo Piezas, mantener presionado encoder **3 segundos**
2. Seleccionar cantidad de piezas a usar (10, 20, 50, 100, 200)
3. Colocar esa cantidad exacta de piezas en la báscula
4. Esperar conteo regresivo (3 segundos)
5. El sistema calculará el peso por pieza automáticamente

#### Funcionamiento
- Muestra: **"Piezas: X"**
- Calcula automáticamente número de piezas según el peso
- Redondeo inteligente: ≥ 0.85 redondea hacia arriba

#### Regresar a Modo Peso
- Mantener presionado encoder **5 segundos**

### Opción Red
- Muestra la IP actual de la báscula
- IP se guarda en EEPROM y se solicita en cada conexión
- Si no está disponible, obtiene nueva IP por DHCP

### Opción Reiniciar
- Borra toda la configuración guardada
- Reinicia el ESP32
- Requerirá nueva calibración

## Datos Almacenados en EEPROM
- Factor de calibración
- Peso por pieza (modo conteo)
- Dirección IP solicitada
- Persistencia: los datos se mantienen aunque se desconecte la alimentación

## Solución de Problemas

### LCD no muestra nada
- Verificar conexiones I2C
- Ajustar potenciómetro de contraste en módulo I2C
- Verificar dirección I2C (0x27 o 0x3F)

### Lecturas inestables del peso
- Calibrar nuevamente
- Verificar que la celda de carga esté bien montada
- Colocar báscula en superficie estable
- Alejar de fuentes de vibración

### Encoder no responde
- Verificar conexiones
- Probar invertir pines CLK y DT

### WiFi no conecta
- Verificar credenciales en el código
- Verificar que la red sea 2.4GHz (ESP32 no soporta 5GHz)
- Revisar monitor serial para mensajes de error

### Conteo de piezas incorrecto
- Recalibrar peso por pieza
- Asegurar que las piezas sean uniformes
- Usar mayor cantidad de piezas en calibración para mejor precisión

## Monitor Serial
Abrir monitor serial a **115200 baudios** para ver información de depuración.

## Notas Importantes
- Siempre calibrar antes del primer uso
- Mantener báscula en superficie nivelada
- No exceder la capacidad máxima de la celda de carga
- Proteger de humedad y polvo
- El peso por pieza se guarda entre sesiones

## Mejoras Futuras Posibles
- Servidor web para ver peso remotamente
- Envío de datos a servidor MQTT
- Registro de pesadas en SD
- Múltiples perfiles de piezas

## Licencia
Código libre para uso personal y educativo.

---
**Desarrollado para ESP32 DevKit V1**
