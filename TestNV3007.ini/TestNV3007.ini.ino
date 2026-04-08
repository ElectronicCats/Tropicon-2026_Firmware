#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>

// === LED ===//
#define PIN_LED  8
#define NUM_LEDS 1

Adafruit_NeoPixel tiraLed(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);

// ESP32-C6-DevKitC-1 Recommended Pins
#define TFT_MOSI    19 // SPI MOSI (SDA)
#define TFT_CLK     21 // SPI SCLK
#define TFT_DC      22 // Data/Command
#define TFT_RST     23 // Reset
#define TFT_CS      18 // Chip Select
#define TFT_BL      15  // Backlight
#define TFT_MISO    -1 // No usado

// --- Configuración del Bus SPI e Instancia de la Pantalla ---
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS, TFT_CLK, TFT_MOSI, TFT_MISO);
// Usando la secuencia de inicialización alternativa nv3007_279_init_operations para evitar patrones de líneas
Arduino_NV3007 *gfx = new Arduino_NV3007(
    bus, TFT_RST, 1 /* rotation */, false /* IPS */,
    NV3007_TFTWIDTH, NV3007_TFTHEIGHT,
    0 /* col_offset1 */, 0 /* row_offset1 */, 0 /* col_offset2 */, 0 /* row_offset2 */,
    nv3007_279_init_operations, sizeof(nv3007_279_init_operations)
);

void setup(void)
{
    // Inicializar LED
    tiraLed.begin();
    tiraLed.clear();
    tiraLed.setPixelColor(0, tiraLed.Color(0, 0, 255)); // Azul
    tiraLed.show();

    Serial.begin(115200);
    delay(2000);
    Serial.println("Iniciando pantalla NV3007 con ESP32-C6...");

    // Configurar backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Inicializar pantalla con frecuencia de 20MHz para mayor estabilidad
    if (!gfx->begin(20000000))
    {
        tiraLed.clear();
        tiraLed.setPixelColor(0, tiraLed.Color(255, 0, 0)); // Rojo
        tiraLed.show();
        Serial.println("¡Error al inicializar la pantalla!");
        while (1) { delay(1000); }
    }

    tiraLed.clear();
    tiraLed.setPixelColor(0, tiraLed.Color(0, 255, 0)); // Verde
    tiraLed.show();

    Serial.println("Pantalla inicializada correctamente.");
    Serial.print("Resolución: ");
    Serial.print(gfx->width());
    Serial.print("x");
    Serial.println(gfx->height());

    // Mostrar mensajes
    gfx->fillScreen(RGB565_BLACK);
    gfx->setCursor(10, 50);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->println("Hola, ESP32-C6!");

    gfx->setCursor(10, 90);
    gfx->setTextColor(RGB565_GREEN);
    gfx->setTextSize(1);
    gfx->println("NV3007 funcionando");

    gfx->setCursor(10, 110);
    gfx->setTextColor(RGB565_CYAN);
    gfx->printf("Res: %dx%d", gfx->width(), gfx->height());
    
    // Marco decorativo
    gfx->drawRect(5, 5, gfx->width() - 10, gfx->height() - 10, RGB565_WHITE);
    gfx->drawRect(8, 8, gfx->width() - 16, gfx->height() - 16, RGB565_BLUE);
}

void loop(void)
{
  gfx->setCursor(random(gfx->width()), random(gfx->height()));
  gfx->setTextColor(random(0xffff), random(0xffff));
  gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
  gfx->println("Hello World!");

  delay(1000); // 1 second
}