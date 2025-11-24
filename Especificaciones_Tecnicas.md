# Especificaciones Técnicas - Báscula ESP32

## Diagrama de Pines

```
╔══════════════════════════════════════════════════════════╗
║                    ESP32 DevKit V1                        ║
╠══════════════════════════════════════════════════════════╣
║                                                           ║
║  GPIO 16 ────────► HX711 DOUT                            ║
║  GPIO 4  ────────► HX711 SCK                             ║
║                                                           ║
║  GPIO 21 ────────► LCD I2C SDA                           ║
║  GPIO 22 ────────► LCD I2C SCL                           ║
║                                                           ║
║  GPIO 25 ────────► Encoder CLK                           ║
║  GPIO 26 ────────► Encoder DT                            ║
║  GPIO 27 ────────► Encoder SW (botón)                    ║
║                                                           ║
║  5V      ────────► HX711 VCC, LCD VCC                    ║
║  3.3V    ────────► Encoder VCC                           ║
║  GND     ────────► Común a todos los módulos             ║
║                                                           ║
╚══════════════════════════════════════════════════════════╝
```

## Características del Sistema

### Hardware
- **Microcontrolador**: ESP32 DevKit V1 (dual-core, 240MHz)
- **Sensor**: HX711 ADC 24-bit para celdas de carga
- **Display**: LCD 16x2 con interfaz I2C
- **Interface**: Encoder rotatorio con botón integrado
- **Conectividad**: WiFi 802.11 b/g/n (2.4GHz)

### Capacidades
- **Resolución**: Depende de la celda de carga utilizada
- **Precisión**: ±0.1g (con calibración adecuada)
- **Frecuencia de actualización**: 10 Hz
- **Modos de operación**: 
  - Báscula tradicional
  - Contador de piezas
- **Almacenamiento persistente**: EEPROM virtual ESP32

## Flujo de Operación

### Diagrama de Estados

```
                    ┌─────────────────┐
                    │  INICIO SISTEMA │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  Cargar EEPROM  │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  Conectar WiFi  │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  Tarar Balanza  │
                    └────────┬────────┘
                             │
                  ┌──────────▼──────────┐
                  │ ¿Calibración existe?│
                  └──────┬───────┬──────┘
                         NO      SÍ
                         │       │
                 ┌───────▼──┐    │
                 │ CALIBRAR │    │
                 └───────┬──┘    │
                         │       │
                    ┌────▼───────▼────┐
                    │   MODO PESO     │◄─────────┐
                    └────┬────────────┘          │
                         │                       │
            Presionar 5s │                       │ Presionar 5s
                         │                       │
                    ┌────▼────────────┐          │
                    │  MODO PIEZAS    │──────────┘
                    └────┬────────────┘
                         │
            Presionar 3s │
                         │
                    ┌────▼────────────┐
                    │ MENÚ PRINCIPAL  │
                    └─────────────────┘
                    │    │         │
            ┌───────┘    │         └────────┐
            │            │                  │
       ┌────▼─────┐ ┌───▼────┐    ┌───────▼────┐
       │CALIBRAR  │ │  RED   │    │ REINICIAR  │
       └──────────┘ └────────┘    └────────────┘
```

## Gestión de Memoria EEPROM

### Namespace: "bascula"

| Clave       | Tipo  | Tamaño | Descripción                    | Valor por defecto |
|-------------|-------|--------|--------------------------------|-------------------|
| factor      | float | 4 bytes| Factor de calibración HX711    | 1.0               |
| pesoPieza   | float | 4 bytes| Peso unitario de pieza (g)     | 10.0              |
| ip0         | uint  | 4 bytes| Primer octeto de IP            | 0                 |
| ip1         | uint  | 4 bytes| Segundo octeto de IP           | 0                 |
| ip2         | uint  | 4 bytes| Tercer octeto de IP            | 0                 |
| ip3         | uint  | 4 bytes| Cuarto octeto de IP            | 0                 |

**Tamaño total**: 24 bytes

## Algoritmos Clave

### Cálculo de Factor de Calibración
```
Factor = Lectura_HX711 / Peso_Conocido
```

### Conteo de Piezas con Redondeo Inteligente
```
Piezas_Flotante = Peso_Total / Peso_Por_Pieza

Si (Piezas_Flotante - floor(Piezas_Flotante)) >= 0.85:
    Piezas = ceiling(Piezas_Flotante)
Sino:
    Piezas = floor(Piezas_Flotante)
```

### Gestión de Interrupciones del Encoder
```
Interrupción en CLK:
    Si CLK cambió:
        Si DT ≠ CLK:
            encoderPos++  (Giro horario)
        Sino:
            encoderPos--  (Giro antihorario)
```

## Tiempos Críticos

| Evento                    | Duración  | Descripción                          |
|---------------------------|-----------|--------------------------------------|
| Presión botón corta       | < 3s      | Confirmar selección en menús         |
| Presión botón media       | 3-5s      | Abrir menú                           |
| Presión botón larga       | ≥ 5s      | Cambiar modo peso/piezas             |
| Debounce encoder          | 50ms      | Evitar rebotes                       |
| Conteo calibración peso   | 5s        | Tiempo para tara y medición          |
| Conteo calibración piezas | 3s        | Tiempo para medición de muestra      |
| Lecturas HX711            | 10 muestras| Promediado para estabilidad         |
| Actualización LCD         | 100ms     | Refresco de pantalla                 |

## Consumo de Corriente Estimado

| Componente      | Corriente típica | Corriente máxima |
|-----------------|------------------|------------------|
| ESP32 (WiFi ON) | 80-160mA         | 240mA            |
| HX711           | 1.5mA            | 10mA             |
| LCD con backlight| 30mA            | 50mA             |
| Encoder         | < 1mA            | < 1mA            |
| **TOTAL**       | **~120mA**       | **~300mA**       |

**Fuente recomendada**: 5V / 1A mínimo

## Protocolos de Comunicación

### I2C (LCD)
- **Frecuencia**: 100kHz (estándar)
- **Dirección**: 0x27 o 0x3F
- **Pines**: SDA (GPIO21), SCL (GPIO22)

### Serial (HX711)
- **Protocolo**: Propietario HX711
- **Pines**: DOUT (GPIO16), SCK (GPIO4)
- **Frecuencia**: Configurable (típico 80Hz)

### WiFi
- **Estándar**: 802.11 b/g/n
- **Banda**: 2.4GHz únicamente
- **Modo**: Station (STA)
- **IP**: Estática solicitada, DHCP como respaldo

## Requisitos de Precisión

### Para Báscula de Peso
- Celda de carga apropiada para el rango deseado
- Calibración con peso conocido certificado
- Superficie estable y nivelada
- Temperatura ambiente estable (±5°C)

### Para Conteo de Piezas
- Mínimo 10 piezas para calibración
- Piezas uniformes en peso
- Mayor cantidad de piezas → Mayor precisión
- Peso por pieza > 1g recomendado

## Estructura del Código

### Módulos Principales

```
main.ino
│
├── Inicialización
│   ├── setup()
│   ├── inicializarSistema()
│   ├── cargarDatosEEPROM()
│   └── conectarWiFi()
│
├── Loop Principal
│   ├── verificarBoton()
│   ├── mostrarPeso()
│   ├── mostrarPiezas()
│   └── mostrarMenuPrincipal()
│
├── Manejo de Encoder
│   ├── leerEncoder() [ISR]
│   └── verificarBoton()
│
├── Calibración
│   ├── procesarCalibracion()
│   ├── procesarMenuPiezas()
│   └── taraBalanza()
│
├── Utilidades
│   ├── seleccionarOpcion()
│   ├── conteoRegresivo()
│   ├── obtenerPeso()
│   └── guardarDatosEEPROM()
│
└── Red
    └── conectarWiFi()
```

## Casos de Uso

### Caso 1: Pesaje Simple
```
Usuario → Enciende báscula
Sistema → Tara automática
Sistema → Muestra "Peso: 0.0 g"
Usuario → Coloca objeto
Sistema → Muestra "Peso: 125.3 g"
```

### Caso 2: Conteo de Tornillos
```
Usuario → Presiona 5s → Modo Piezas
Usuario → Presiona 3s → Menú calibración
Usuario → Selecciona "100 piezas"
Usuario → Coloca 100 tornillos
Sistema → Calcula: Peso pieza = 2.5g
Sistema → Muestra "Piezas: 100"
Usuario → Añade más tornillos
Sistema → Muestra "Piezas: 157"
```

### Caso 3: Recalibración
```
Usuario → Presiona 3s → Menú
Usuario → Selecciona "Calibracion"
Usuario → Selecciona "200g"
Sistema → "Retire todo peso"
Sistema → Conteo 5-0 → Tara
Sistema → "Coloque 200g"
Sistema → Conteo 5-0 → Calibra
Sistema → "Calibracion exitosa!"
```

## Seguridad y Consideraciones

### Electromagnética
- Alejado de fuentes de interferencia RF
- Cable HX711 corto (< 30cm recomendado)
- Blindaje opcional para entornos ruidosos

### Mecánica
- Celda de carga montada correctamente
- No exceder capacidad máxima
- Protección contra sobrecarga recomendada

### Eléctrica
- Fuente estabilizada 5V
- Protección contra inversión de polaridad
- Decoupling capacitors en fuente

---

**Versión del Documento**: 1.0  
**Fecha**: Noviembre 2025  
**Plataforma**: Arduino IDE + ESP32 Core
