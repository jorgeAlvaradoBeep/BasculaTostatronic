/*
 * Báscula Digital con ESP32 DevKit V1
 * - HX711 con celda de carga
 * - LCD 16x2 I2C
 * - Encoder KY-040
 * - Modo peso y modo conteo de piezas
 * - Calibración y configuración WiFi
 */

#include <HX711.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Preferences.h>

// ========== CONFIGURACIÓN DE PINES ==========
#define HX711_DOUT  16
#define HX711_SCK   4
#define ENCODER_CLK 25
#define ENCODER_DT  26
#define ENCODER_SW  27

// ========== CONFIGURACIÓN WIFI ==========
const char* ssid = "Tostatronic";          // Cambiar por tu red
const char* password = "Tostatronic1995%";    // Cambiar por tu contraseña

// ========== OBJETOS ==========
HX711 balanza;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Dirección I2C común, cambiar a 0x3F si no funciona
Preferences preferences;

// ========== VARIABLES DE CALIBRACIÓN ==========
float factorCalibracion = 1.0;
float pesoCalibrado = 0;
bool calibracionRequerida = true;

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
void taraBalanza();
void conteoRegresivo(int segundos, String mensaje);
int seleccionarOpcion(String* opciones, int numOpciones, String titulo);
float obtenerPeso();

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  
  // Inicializar LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inicializando...");
  
  // Inicializar pines del encoder
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  // Interrupciones para el encoder
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), leerEncoder, CHANGE);
  
  // Inicializar HX711
  balanza.begin(HX711_DOUT, HX711_SCK);
  delay(1000);
  
  // Cargar datos de EEPROM
  cargarDatosEEPROM();
  
  // Conectar WiFi
  conectarWiFi();
  
  // Inicializar balanza
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tarando...");
  delay(1000);
  taraBalanza();
  
  // Aplicar factor de calibración si existe
  if (factorCalibracion != 1.0) {
    balanza.set_scale(factorCalibracion);
    calibracionRequerida = false;
  }
  
  lcd.clear();
  
  // Si no hay calibración, solicitarla
  if (calibracionRequerida) {
    lcd.setCursor(0, 0);
    lcd.print("Calibracion");
    lcd.setCursor(0, 1);
    lcd.print("requerida!");
    delay(2000);
    procesarCalibracion();
  }
  
  modoActual = MODO_PESO;
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
  float peso = obtenerPeso();
  
  lcd.setCursor(0, 0);
  lcd.print("Peso:           ");
  lcd.setCursor(0, 1);
  lcd.print(peso, 1);
  lcd.print(" g     ");
}

// ========== OBTENER PESO ==========
float obtenerPeso() {
  if (balanza.is_ready()) {
    float peso = balanza.get_units(10);
    return peso;
  }
  return 0.0;
}

// ========== MOSTRAR PIEZAS ==========
void mostrarPiezas() {
  float peso = obtenerPeso();
  
  // Calcular número de piezas
  float piezasFloat = peso / pesoPorPieza;
  
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
  
  // Seleccionar peso de calibración
  String opciones[] = {"50g", "100g", "200g", "500g", "1000g"};
  float pesos[] = {50, 100, 200, 500, 1000};
  
  int seleccion = seleccionarOpcion(opciones, 5, "Peso calib:");
  pesoCalibrado = pesos[seleccion];
  
  // Mensaje: Quitar peso
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Retire todo");
  lcd.setCursor(0, 1);
  lcd.print("peso");
  delay(3000);
  
  conteoRegresivo(5, "Tarando en:");
  
  // Tarar
  balanza.tare(20);
  
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
  long lectura = balanza.get_units(20);
  factorCalibracion = lectura / pesoCalibrado;
  balanza.set_scale(factorCalibracion);
  
  // Guardar en EEPROM
  guardarDatosEEPROM();
  calibracionRequerida = false;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibracion");
  lcd.setCursor(0, 1);
  lcd.print("exitosa!");
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

// ========== TARAR BALANZA ==========
void taraBalanza() {
  balanza.tare(20);
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
