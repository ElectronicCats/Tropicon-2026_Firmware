#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>

// ESP32-CS3-DevKitC-1 Customized Pins
#define TFT_MOSI    11 // SPI MOSI (SDA)
#define TFT_CLK     12 // SPI SCLK
#define TFT_DC       9 // Data/Command
#define TFT_RST     21 // Reset
#define TFT_CS      10 // Chip Select
#define TFT_BL       2 // Backlight
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
    Serial.begin(115200);
    delay(2000);
    Serial.println("Iniciando pantalla NV3007 con ESP32-S3...");

    // Configurar backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Inicializar pantalla con frecuencia de 20MHz para mayor estabilidad
    if (!gfx->begin(20000000))
    {
        Serial.println("¡Error al inicializar la pantalla!");
        while (1) { delay(1000); }
    }

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
    gfx->println("Hola, ESP32-S3!");

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