/*
 * Báscula Digital con ESP32 DevKit V1
 * - HX711 con celda de carga
 * - LCD 16x2 I2C
 * - Encoder KY-040
 * - Modo peso y modo conteo de piezas
 * - Calibración y configuración WiFi
 * 
 * CONEXIONES HX711:
 * - VCC -> 5V (IMPORTANTE: algunos módulos necesitan 5V, no 3.3V)
 * - GND -> GND
 * - DT (DOUT) -> GPIO 16
 * - SCK -> GPIO 4
 */

#include <HX711.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Preferences.h>

// ========== CONFIGURACIÓN DE PINES ==========
// Pines HX711 - VERIFICAR CONEXIONES
#define HX711_DOUT  16   // DT del HX711
#define HX711_SCK   4    // SCK del HX711

// Pines Encoder KY-040
#define ENCODER_CLK 25
#define ENCODER_DT  26
#define ENCODER_SW  27

// ========== CONFIGURACIÓN WIFI ==========
const char* ssid = "TU_RED_WIFI";          // Cambiar por tu red
const char* password = "TU_CONTRASEÑA";    // Cambiar por tu contraseña

// ========== OBJETOS ==========
HX711 balanza;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Dirección I2C común, cambiar a 0x3F si no funciona
Preferences preferences;

// ========== VARIABLES DE CALIBRACIÓN ==========
float factorCalibracion = 1.0;
float pesoCalibrado = 0;
bool calibracionRequerida = true;
bool hx711Conectado = false;

// ========== VARIABLES DE ENCODER ==========
volatile int encoderPos = 0;
int lastEncoderPos = 0;
unsigned long lastEncoderTime = 0;
bool lastCLK = HIGH;

// ========== VARIABLES DE BOTÓN ==========
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
bool buttonHandled = false;

// ========== MODOS Y ESTADOS ==========
enum Modo { MODO_PESO, MODO_PIEZAS, MENU_PRINCIPAL, MENU_CALIBRACION, MENU_PIEZAS };
Modo modoActual = MODO_PESO;
int opcionMenu = 0;

// ========== VARIABLES DE CONTEO DE PIEZAS ==========
float pesoPorPieza = 10.0;  // Peso por defecto
int cantidadPiezas = 0;

// ========== VARIABLES DE ESTABILIDAD ==========
float pesoMostrado = 0.0;       // Último peso mostrado en LCD
int piezasMostradas = 0;        // Últimas piezas mostradas en LCD
float umbralCambio = 1.0;       // Variación mínima para actualizar (1 gramo)

// ========== VARIABLES DE RED ==========
IPAddress ipGuardada(0, 0, 0, 0);
String ipActualStr = "";

// ========== PROTOTIPOS DE FUNCIONES ==========
void inicializarSistema();
void leerEncoder();
void verificarBoton();
void mostrarPeso();
void mostrarPiezas();
void mostrarMenuPrincipal();
void procesarMenuPrincipal();
void procesarCalibracion();
void procesarMenuPiezas();
void guardarDatosEEPROM();
void cargarDatosEEPROM();
void conectarWiFi();
void borrarEEPROM();
bool taraBalanzaConTimeout(int timeoutMs);
void conteoRegresivo(int segundos, String mensaje);
int seleccionarOpcion(String* opciones, int numOpciones, String titulo);
float obtenerPeso();
bool esperarHX711Listo(int timeoutMs);

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========================================");
  Serial.println("Bascula ESP32 - Iniciando...");
  Serial.println("========================================");
  
  // Inicializar LCD
  Wire.begin(21, 22);  // SDA, SCL explícitos
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inicializando...");
  Serial.println("[OK] LCD inicializado");
  
  // Inicializar pines del encoder
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  Serial.println("[OK] Pines encoder configurados");
  
  // Interrupciones para el encoder
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), leerEncoder, CHANGE);
  
  // Inicializar HX711
  Serial.println("\n--- Inicializando HX711 ---");
  Serial.printf("Pin DOUT: GPIO %d\n", HX711_DOUT);
  Serial.printf("Pin SCK:  GPIO %d\n", HX711_SCK);
  
  balanza.begin(HX711_DOUT, HX711_SCK);
  
  lcd.setCursor(0, 1);
  lcd.print("Esperando HX711");
  
  // Esperar a que el HX711 esté listo con timeout
  Serial.println("Esperando que HX711 este listo...");
  hx711Conectado = esperarHX711Listo(5000);  // 5 segundos timeout
  
  if (!hx711Conectado) {
    Serial.println("[ERROR] HX711 NO RESPONDE!");
    Serial.println("Verifica:");
    Serial.println("  1. Conexiones fisicas");
    Serial.println("  2. Alimentacion del HX711 (5V)");
    Serial.println("  3. Pines correctos");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR: HX711");
    lcd.setCursor(0, 1);
    lcd.print("No detectado!");
    
    // Parpadear mensaje de error
    while (!hx711Conectado) {
      delay(2000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Revisar cables");
      lcd.setCursor(0, 1);
      lcd.print("DT->16 SCK->4");
      delay(2000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERROR: HX711");
      lcd.setCursor(0, 1);
      lcd.print("No detectado!");
      
      // Reintentar conexión
      hx711Conectado = esperarHX711Listo(2000);
      if (hx711Conectado) {
        Serial.println("[OK] HX711 detectado en reintento!");
        break;
      }
    }
  }
  
  Serial.println("[OK] HX711 conectado y respondiendo");
  
  // Cargar datos de EEPROM
  cargarDatosEEPROM();
  Serial.printf("[OK] EEPROM cargada - Factor: %.2f, PesoPieza: %.2f\n", factorCalibracion, pesoPorPieza);
  
  // Conectar WiFi
  conectarWiFi();
  
  // Tarar balanza
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tarando...");
  Serial.println("\n--- Tarando balanza ---");
  
  if (taraBalanzaConTimeout(10000)) {  // 10 segundos timeout
    Serial.println("[OK] Tara completada");
  } else {
    Serial.println("[WARN] Tara con timeout - usando valor por defecto");
  }
  
  // Aplicar factor de calibración si existe
  if (factorCalibracion != 1.0) {
    balanza.set_scale(factorCalibracion);
    calibracionRequerida = false;
    Serial.printf("[OK] Factor de calibracion aplicado: %.2f\n", factorCalibracion);
  }
  
  lcd.clear();
  
  // Si no hay calibración, solicitarla
  if (calibracionRequerida) {
    Serial.println("\n[!] Calibracion requerida - iniciando proceso...");
    lcd.setCursor(0, 0);
    lcd.print("Calibracion");
    lcd.setCursor(0, 1);
    lcd.print("requerida!");
    delay(2000);
    procesarCalibracion();
  }
  
  modoActual = MODO_PESO;
  Serial.println("\n========================================");
  Serial.println("Sistema listo - Modo PESO activo");
  Serial.println("========================================\n");
}

// ========== ESPERAR HX711 LISTO ==========
bool esperarHX711Listo(int timeoutMs) {
  unsigned long inicio = millis();
  int intentos = 0;
  
  while (millis() - inicio < timeoutMs) {
    if (balanza.is_ready()) {
      Serial.printf("HX711 listo despues de %d ms (%d intentos)\n", millis() - inicio, intentos);
      return true;
    }
    intentos++;
    delay(10);
    
    // Mostrar progreso cada 500ms
    if (intentos % 50 == 0) {
      Serial.printf("  Esperando HX711... %d ms\n", millis() - inicio);
    }
  }
  
  Serial.printf("Timeout esperando HX711 (%d ms, %d intentos)\n", timeoutMs, intentos);
  return false;
}

// ========== TARA CON TIMEOUT ==========
bool taraBalanzaConTimeout(int timeoutMs) {
  Serial.println("Iniciando tara...");
  
  // Primero verificar que HX711 responde
  if (!esperarHX711Listo(3000)) {
    Serial.println("HX711 no responde para tara");
    return false;
  }
  
  // Hacer tara con pocas muestras para evitar bloqueo
  unsigned long inicio = millis();
  balanza.tare(5);  // Solo 5 muestras en lugar de 20
  
  // Resetear peso mostrado
  pesoMostrado = 0.0;
  
  Serial.printf("Tara completada en %d ms\n", millis() - inicio);
  return true;
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  verificarBoton();
  
  switch (modoActual) {
    case MODO_PESO:
      mostrarPeso();
      break;
      
    case MODO_PIEZAS:
      mostrarPiezas();
      break;
      
    case MENU_PRINCIPAL:
      mostrarMenuPrincipal();
      break;
  }
  
  delay(100);
}

// ========== FUNCIONES DE ENCODER ==========
void IRAM_ATTR leerEncoder() {
  bool clk = digitalRead(ENCODER_CLK);
  bool dt = digitalRead(ENCODER_DT);
  
  if (clk != lastCLK) {
    if (dt != clk) {
      encoderPos++;
    } else {
      encoderPos--;
    }
    lastEncoderTime = millis();
  }
  lastCLK = clk;
}

// ========== VERIFICAR BOTÓN ==========
void verificarBoton() {
  bool botonPresionado = digitalRead(ENCODER_SW) == LOW;
  
  if (botonPresionado && !buttonPressed) {
    buttonPressed = true;
    buttonPressTime = millis();
    buttonHandled = false;
  }
  
  if (!botonPresionado && buttonPressed) {
    unsigned long pressDuration = millis() - buttonPressTime;
    buttonPressed = false;
    
    if (!buttonHandled) {
      // Presión corta (menos de 3 segundos)
      if (pressDuration < 3000) {
        if (modoActual == MENU_PRINCIPAL) {
          procesarMenuPrincipal();
        }
      }
    }
  }
  
  // Verificar presiones largas mientras está presionado
  if (buttonPressed && !buttonHandled) {
    unsigned long pressDuration = millis() - buttonPressTime;
    
    // 3 segundos - Abrir menú
    if (pressDuration >= 3000 && pressDuration < 5000) {
      if (modoActual == MODO_PESO) {
        buttonHandled = true;
        modoActual = MENU_PRINCIPAL;
        opcionMenu = 0;
        encoderPos = 0;
        lcd.clear();
      } else if (modoActual == MODO_PIEZAS) {
        buttonHandled = true;
        procesarMenuPiezas();
      }
    }
    
    // 5 segundos - Cambiar modo
    if (pressDuration >= 5000) {
      buttonHandled = true;
      
      if (modoActual == MODO_PESO) {
        modoActual = MODO_PIEZAS;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Modo: PIEZAS");
        delay(1500);
        lcd.clear();
      } else if (modoActual == MODO_PIEZAS) {
        modoActual = MODO_PESO;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Modo: PESO");
        delay(1500);
        lcd.clear();
      }
    }
  }
}

// ========== MOSTRAR PESO ==========
void mostrarPeso() {
  float pesoLeido = obtenerPeso();
  
  // Solo actualizar si la diferencia es mayor al umbral (1g)
  if (abs(pesoLeido - pesoMostrado) >= umbralCambio) {
    pesoMostrado = pesoLeido;
  }
  
  lcd.setCursor(0, 0);
  lcd.print("Peso:           ");
  lcd.setCursor(0, 1);
  lcd.print(pesoMostrado, 1);
  lcd.print(" g     ");
}

// ========== OBTENER PESO ==========
float obtenerPeso() {
  if (!hx711Conectado) {
    return 0.0;
  }
  
  // Verificar si está listo con timeout corto
  unsigned long inicio = millis();
  while (!balanza.is_ready()) {
    if (millis() - inicio > 500) {  // 500ms timeout
      Serial.println("[WARN] HX711 no responde en obtenerPeso");
      return 0.0;
    }
    delay(1);
  }
  
  float peso = balanza.get_units(5);  // 5 muestras para rapidez
  return peso;
}

// ========== MOSTRAR PIEZAS ==========
void mostrarPiezas() {
  float pesoLeido = obtenerPeso();
  
  // Solo actualizar si la diferencia es mayor al umbral (1g)
  if (abs(pesoLeido - pesoMostrado) >= umbralCambio) {
    pesoMostrado = pesoLeido;
  }
  
  // Calcular número de piezas basado en peso estable
  float piezasFloat = pesoMostrado / pesoPorPieza;
  
  // Redondeo inteligente
  if (piezasFloat - (int)piezasFloat >= 0.85) {
    cantidadPiezas = (int)piezasFloat + 1;
  } else {
    cantidadPiezas = (int)piezasFloat;
  }
  
  if (cantidadPiezas < 0) cantidadPiezas = 0;
  
  lcd.setCursor(0, 0);
  lcd.print("Piezas:         ");
  lcd.setCursor(0, 1);
  lcd.print(cantidadPiezas);
  lcd.print("            ");
}

// ========== MENÚ PRINCIPAL ==========
void mostrarMenuPrincipal() {
  String opciones[] = {"Calibracion", "Red", "Reiniciar"};
  
  // Actualizar posición del menú
  if (encoderPos != lastEncoderPos) {
    opcionMenu += (encoderPos > lastEncoderPos) ? 1 : -1;
    if (opcionMenu < 0) opcionMenu = 2;
    if (opcionMenu > 2) opcionMenu = 0;
    lastEncoderPos = encoderPos;
  }
  
  lcd.setCursor(0, 0);
  lcd.print("Menu:           ");
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(opciones[opcionMenu]);
  lcd.print("          ");
}

// ========== PROCESAR MENÚ PRINCIPAL ==========
void procesarMenuPrincipal() {
  switch (opcionMenu) {
    case 0: // Calibración
      procesarCalibracion();
      break;
      
    case 1: // Red
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("IP Actual:");
      lcd.setCursor(0, 1);
      if (ipActualStr.length() > 16) {
        lcd.print(ipActualStr.substring(0, 16));
      } else {
        lcd.print(ipActualStr);
      }
      delay(5000);
      modoActual = MODO_PESO;
      lcd.clear();
      break;
      
    case 2: // Reiniciar
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Borrando datos");
      lcd.setCursor(0, 1);
      lcd.print("y reiniciando..");
      borrarEEPROM();
      delay(2000);
      ESP.restart();
      break;
  }
}

// ========== PROCESAR CALIBRACIÓN ==========
void procesarCalibracion() {
  lcd.clear();
  Serial.println("\n--- Iniciando calibracion ---");
  
  // Seleccionar peso de calibración
  String opciones[] = {"50g", "100g", "200g", "500g", "1000g"};
  float pesos[] = {50, 100, 200, 500, 1000};
  
  int seleccion = seleccionarOpcion(opciones, 5, "Peso calib:");
  pesoCalibrado = pesos[seleccion];
  Serial.printf("Peso seleccionado: %.0f g\n", pesoCalibrado);
  
  // Mensaje: Quitar peso
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Retire todo");
  lcd.setCursor(0, 1);
  lcd.print("peso");
  delay(3000);
  
  conteoRegresivo(5, "Tarando en:");
  
  // Tarar con verificación
  Serial.println("Ejecutando tara...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tarando...");
  
  if (esperarHX711Listo(3000)) {
    balanza.tare(10);
    Serial.println("Tara completada");
  } else {
    Serial.println("[ERROR] HX711 no responde para tara");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error HX711");
    delay(2000);
    modoActual = MODO_PESO;
    return;
  }
  
  // Mensaje: Colocar peso
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Coloque ");
  lcd.print((int)pesoCalibrado);
  lcd.print("g");
  lcd.setCursor(0, 1);
  lcd.print("en la bascula");
  delay(3000);
  
  conteoRegresivo(5, "Calibrando en:");
  
  // Obtener lectura y calcular factor
  Serial.println("Leyendo valor de calibracion...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Leyendo...");
  
  if (!esperarHX711Listo(3000)) {
    Serial.println("[ERROR] HX711 no responde para lectura");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error lectura");
    delay(2000);
    modoActual = MODO_PESO;
    return;
  }
  
  // Obtener lectura raw (sin escala)
  balanza.set_scale(1);  // Escala 1:1 para lectura raw
  long lectura = balanza.get_units(10);
  
  Serial.printf("Lectura raw: %ld\n", lectura);
  Serial.printf("Peso conocido: %.0f g\n", pesoCalibrado);
  
  if (lectura == 0) {
    Serial.println("[ERROR] Lectura es 0 - verificar celda de carga");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: lectura 0");
    lcd.setCursor(0, 1);
    lcd.print("Revisar celda");
    delay(3000);
    modoActual = MODO_PESO;
    return;
  }
  
  factorCalibracion = (float)lectura / pesoCalibrado;
  Serial.printf("Factor calculado: %.2f\n", factorCalibracion);
  
  balanza.set_scale(factorCalibracion);
  
  // Resetear peso mostrado
  pesoMostrado = 0.0;
  
  // Guardar en EEPROM
  guardarDatosEEPROM();
  calibracionRequerida = false;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibracion");
  lcd.setCursor(0, 1);
  lcd.print("exitosa!");
  Serial.println("[OK] Calibracion exitosa!");
  delay(2000);
  
  modoActual = MODO_PESO;
  lcd.clear();
}

// ========== MENÚ DE CALIBRACIÓN DE PIEZAS ==========
void procesarMenuPiezas() {
  lcd.clear();
  
  // Seleccionar cantidad de piezas
  String opciones[] = {"10", "20", "50", "100", "200"};
  int cantidades[] = {10, 20, 50, 100, 200};
  
  int seleccion = seleccionarOpcion(opciones, 5, "Cant piezas:");
  int cantSeleccionada = cantidades[seleccion];
  
  // Mensaje: Colocar piezas
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Coloque ");
  lcd.print(cantSeleccionada);
  lcd.setCursor(0, 1);
  lcd.print("piezas");
  delay(3000);
  
  conteoRegresivo(3, "Midiendo en:");
  
  // Obtener peso total y calcular peso por pieza
  float pesoTotal = obtenerPeso();
  pesoPorPieza = pesoTotal / cantSeleccionada;
  
  // Guardar en EEPROM
  guardarDatosEEPROM();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibracion OK");
  lcd.setCursor(0, 1);
  lcd.print("Pieza:");
  lcd.print(pesoPorPieza, 2);
  lcd.print("g");
  delay(3000);
  
  lcd.clear();
}

// ========== SELECCIONAR OPCIÓN ==========
int seleccionarOpcion(String* opciones, int numOpciones, String titulo) {
  int seleccion = 0;
  encoderPos = 0;
  lastEncoderPos = 0;
  bool confirmado = false;
  
  while (!confirmado) {
    // Actualizar selección con encoder
    if (encoderPos != lastEncoderPos) {
      seleccion += (encoderPos > lastEncoderPos) ? 1 : -1;
      if (seleccion < 0) seleccion = numOpciones - 1;
      if (seleccion >= numOpciones) seleccion = 0;
      lastEncoderPos = encoderPos;
    }
    
    // Mostrar opción actual
    lcd.setCursor(0, 0);
    lcd.print(titulo);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(opciones[seleccion]);
    lcd.print("          ");
    
    // Verificar confirmación
    if (digitalRead(ENCODER_SW) == LOW) {
      delay(50);
      while (digitalRead(ENCODER_SW) == LOW);
      confirmado = true;
    }
    
    delay(100);
  }
  
  return seleccion;
}

// ========== CONTEO REGRESIVO ==========
void conteoRegresivo(int segundos, String mensaje) {
  for (int i = segundos; i >= 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(mensaje);
    lcd.setCursor(0, 1);
    lcd.print(i);
    lcd.print(" segundos");
    delay(1000);
  }
}

// ========== TARAR BALANZA (wrapper) ==========
void taraBalanza() {
  taraBalanzaConTimeout(5000);
}

// ========== GUARDAR DATOS EN EEPROM ==========
void guardarDatosEEPROM() {
  preferences.begin("bascula", false);
  preferences.putFloat("factor", factorCalibracion);
  preferences.putFloat("pesoPieza", pesoPorPieza);
  preferences.putUInt("ip0", ipGuardada[0]);
  preferences.putUInt("ip1", ipGuardada[1]);
  preferences.putUInt("ip2", ipGuardada[2]);
  preferences.putUInt("ip3", ipGuardada[3]);
  preferences.end();
}

// ========== CARGAR DATOS DE EEPROM ==========
void cargarDatosEEPROM() {
  preferences.begin("bascula", true);
  factorCalibracion = preferences.getFloat("factor", 1.0);
  pesoPorPieza = preferences.getFloat("pesoPieza", 10.0);
  ipGuardada[0] = preferences.getUInt("ip0", 0);
  ipGuardada[1] = preferences.getUInt("ip1", 0);
  ipGuardada[2] = preferences.getUInt("ip2", 0);
  ipGuardada[3] = preferences.getUInt("ip3", 0);
  preferences.end();
  
  if (factorCalibracion == 1.0) {
    calibracionRequerida = true;
  }
}

// ========== BORRAR EEPROM ==========
void borrarEEPROM() {
  preferences.begin("bascula", false);
  preferences.clear();
  preferences.end();
}

// ========== CONECTAR WIFI ==========
void conectarWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");
  
  WiFi.mode(WIFI_STA);
  
  // Si hay IP guardada, intentar usarla
  if (ipGuardada[0] != 0) {
    IPAddress gateway(ipGuardada[0], ipGuardada[1], ipGuardada[2], 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ipGuardada, gateway, subnet);
  }
  
  WiFi.begin(ssid, password);
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print(".");
    intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ipActual = WiFi.localIP();
    ipActualStr = ipActual.toString();
    
    // Si la IP cambió, guardarla
    if (ipActual != ipGuardada) {
      ipGuardada = ipActual;
      guardarDatosEEPROM();
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi OK");
    lcd.setCursor(0, 1);
    lcd.print(ipActualStr);
    delay(2000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi: Sin conex");
    ipActualStr = "Sin conexion";
    delay(2000);
  }
}
