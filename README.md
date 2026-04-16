<div align="center" markdown="1">

<img src=".github/meshtastic_logo.png" alt="Meshtastic Logo" width="80"/>
<h1>Tropicon 2026 — Firmware</h1>

Firmware personalizado de Meshtastic para el badge del evento **Tropicon 2026**, desarrollado por **Electronic Cats**. Incluye integración de agenda de pláticas, talleres, monitor AIS de embarcaciones, telemetría ambiental y mensajería LoRa.

</div>

---

## Tabla de contenidos

- [Tabla de contenidos](#tabla-de-contenidos)
- [Primeros pasos](#primeros-pasos)
- [Personalizar tu nombre en el badge](#personalizar-tu-nombre-en-el-badge)
	- [Pasos para cambiar el nombre](#pasos-para-cambiar-el-nombre)
- [Botones y navegación general](#botones-y-navegación-general)
- [Pantallas del dispositivo](#pantallas-del-dispositivo)
- [Menú de Acciones (Home Action)](#menú-de-acciones-home-action)
- [Agenda de Pláticas y Talleres (Talks)](#agenda-de-pláticas-y-talleres-talks)
	- [Vista lista](#vista-lista)
	- [Vista detalle](#vista-detalle)
	- [Marcadores de interés y notificaciones](#marcadores-de-interés-y-notificaciones)
- [Monitor AIS de Embarcaciones](#monitor-ais-de-embarcaciones)
	- [Información mostrada](#información-mostrada)
- [Mensajes — Recibir y Enviar](#mensajes--recibir-y-enviar)
	- [Navegar a la pantalla de mensajes](#navegar-a-la-pantalla-de-mensajes)
	- [Ver mensajes](#ver-mensajes)
	- [Enviar un mensaje (nuevo)](#enviar-un-mensaje-nuevo)
	- [Responder a un mensaje recibido](#responder-a-un-mensaje-recibido)
	- [Otras opciones del menú Message Action](#otras-opciones-del-menú-message-action)
- [Telemetría del Entorno](#telemetría-del-entorno)
- [Conexión por Bluetooth](#conexión-por-bluetooth)
	- [Pasos para emparejar](#pasos-para-emparejar)
- [Set up the Build Environment](#set-up-the-build-environment)
- [Flasheo del dispositivo](#flasheo-del-dispositivo)
	- [Paso 1 — Subir el sistema de archivos (SPIFFS/LittleFS)](#paso-1--subir-el-sistema-de-archivos-spiffslittlefs)
	- [Paso 2 — Subir el firmware](#paso-2--subir-el-firmware)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Personalizar y modificar el código](#personalizar-y-modificar-el-código)
	- [Requisitos previos](#requisitos-previos)
	- [Cambios comunes y dónde hacerlos](#cambios-comunes-y-dónde-hacerlos)
		- [Modificar la agenda de pláticas](#modificar-la-agenda-de-pláticas)
		- [Agregar o cambiar imágenes de ponentes](#agregar-o-cambiar-imágenes-de-ponentes)
		- [Modificar la lógica de la agenda](#modificar-la-lógica-de-la-agenda)
		- [Modificar cómo se ve la agenda en pantalla](#modificar-cómo-se-ve-la-agenda-en-pantalla)
		- [Modificar la pantalla AIS](#modificar-la-pantalla-ais)
		- [Agregar nuevas opciones al menú](#agregar-nuevas-opciones-al-menú)
	- [Flujo de trabajo recomendado](#flujo-de-trabajo-recomendado)
	- [Recursos útiles](#recursos-útiles)

---

## Primeros pasos

Al encender el badge por primera vez:

1. La pantalla mostrará el logo de Meshtastic y luego la pantalla **Home**.
2. El nombre del dispositivo (ej. `Meshtastic-1234`) aparece en la pantalla Home — anótalo para identificarlo en la app Meshtastic.
3. Usa los botones **RIGHT** y **LEFT** para navegar entre las distintas pantallas de información.
4. Para abrir opciones y menús en cualquier pantalla, mantén presionado el botón **UP** por ~1 segundo.

> [!NOTE]
> El badge funciona de forma autónoma. No necesitas conectarlo a un teléfono para ver la agenda, los mensajes LoRa o las métricas del entorno.

---

## Personalizar tu nombre en el badge

Por defecto, el badge muestra un nombre generado automáticamente (ej. `Meshtastic-1234`) en la pantalla **Home** y en los mensajes que envías por la malla LoRa. Puedes cambiarlo a tu nombre o apodo desde la app Meshtastic.

### Pasos para cambiar el nombre

1. Conecta tu badge a la app **Meshtastic** vía Bluetooth ([ver instrucciones](#conexión-por-bluetooth)).
2. En la app, abre la sección de configuración de tu nodo. En la mayoría de las versiones se accede desde el ícono de llave o engrane junto al nombre del dispositivo.
3. Dentro de la configuración, busca la sección de `Configuración del Dispositivo` > `Usuario` o `User`.
4. Busca los campos:
   - **Long Name** — nombre completo que aparece en la pantalla Home del badge y en los mensajes.
   - **Short Name** — abreviatura de 1 a 4 caracteres usada en mapas y listas de nodos.
5. Escribe tu nombre o apodo en **Long Name** y una abreviatura en **Short Name**.
6. Guarda los cambios desde la app.

El badge recibe la actualización vía Bluetooth y muestra el nuevo nombre en la pantalla **Home** de inmediato, sin necesidad de reiniciarlo.

> [!TIP]
> Elige un nombre corto pero reconocible — otros asistentes al evento verán ese nombre cuando reciban tus mensajes en la malla LoRa o cuando tu badge aparezca en la lista de nodos de sus dispositivos.

---

## Botones y navegación general

El badge cuenta con cuatro botones físicos:

| Botón | Pulsación corta | Pulsación larga (~1 s) |
|-------|----------------|------------------------|
| **UP** | Pantalla anterior / Ítem anterior | Abrir menú de acciones de la pantalla actual |
| **DOWN** | Pantalla siguiente / Ítem siguiente | — |
| **LEFT** | Izquierda / Día anterior (en Talks) | — |
| **RIGHT** | Derecha / Siguiente stage (en Talks) | — |

> [!TIP]
> El **botón UP largo** es la acción principal de cada pantalla. Desde cualquier pantalla, úsalo para acceder a las opciones disponibles: enviar mensajes, navegar a la agenda, configurar LoRa, etc.

---

## Pantallas del dispositivo

Navega entre pantallas con **UP** (anterior) y **DOWN** (siguiente). El ciclo completo es:

| # | Pantalla | Descripción |
|---|----------|-------------|
| 1 | **Talks** | Agenda de pláticas y talleres de Tropicon 2026 |
| 2 | **AIS** | Monitor de embarcaciones (Automatic Identification System) |
| 3 | **Clock** | Reloj digital o analógico |
| 4 | **Home** | Estado del nodo: nombre, batería y canal LoRa |
| 5 | **Messages** | Mensajes de texto recibidos y enviados en la malla LoRa |
| 6 | **Node List** | Lista de nodos detectados en la malla |
| 7 | **Telemetry** | Métricas del entorno: Temperatura y Humedad (sensor AHT10) |
| 8 | **GPS** | Brújula y coordenadas (deshabilitado — sin módulo GPS) |
| 9 | **LoRa** | Estadísticas de radio: SNR, RSSI y frecuencia |
| 10 | **System** | Información de sistema: uptime y memoria libre |

---

## Menú de Acciones (Home Action)

Desde la pantalla **Home**, presiona **UP largo** para abrir el menú *"Home Action"*:

| Opción | Qué hace |
|--------|----------|
| **Back** | Cierra el menú sin cambios |
| **Sleep Screen** | Apaga la pantalla para ahorrar batería (cualquier botón la reactiva) |
| **Talks** | Abre directamente la agenda de pláticas y talleres |

---

## Agenda de Pláticas y Talleres (Talks)

La agenda carga automáticamente los datos desde la flash del dispositivo:

| Archivo | Contenido |
|---------|-----------|
| `/schedule.json` | Pláticas principales (Track 1 y Track Villas) |
| `/talleres.json` | Talleres simultáneos (Salón Maya, La Isla, Suite 1, Suite 2) |

Para acceder a la agenda puedes:
- Navegar con **UP/DOWN** hasta llegar a la pantalla **Talks**, o
- Desde cualquier pantalla, abrir el menú con **UP largo** y seleccionar **Talks**.

### Vista lista

Muestra el listado de pláticas del día y stage (escenario) actualmente seleccionados. El encabezado indica `Día — Stage`.

| Acción | Resultado |
|--------|-----------|
| **UP** corto | Sube al ítem anterior de la lista |
| **UP** corto (en la primera plática) | Sale de la agenda y regresa al menú principal |
| **DOWN** | Baja al ítem siguiente de la lista |
| **LEFT** | Cambia al día anterior (cíclico); resetea el stage y la selección |
| **RIGHT** | Avanza al siguiente stage; si ya es el último stage, avanza al siguiente día |
| **UP largo** | Entra a la **vista detalle** de la plática seleccionada |

> [!TIP]
> Para **salir de la agenda en cualquier momento**, presiona **UP** cuando estés en la primera plática de la lista — el badge regresará automáticamente al menú principal de pantallas.

### Vista detalle

Muestra el título completo, nombre del ponente, horario, sala, imagen del ponente y el marcador de interés actual.

| Acción | Resultado |
|--------|-----------|
| **UP** corto | Regresa a la vista lista |
| **DOWN** | Avanza a la siguiente plática (permanece en vista detalle) |
| **LEFT** | Retrocede a la plática anterior (cambia de stage/día si es necesario) |
| **RIGHT** | Avanza a la plática siguiente (cambia de stage/día si es necesario) |
| **UP largo** | Cicla el estado de interés: `Sin interés → Asistir → Tal vez → Saltar` |

### Marcadores de interés y notificaciones

Los marcadores se muestran en la esquina superior derecha de cada plática en la vista lista:

| Marcador | Estado | Descripción |
|----------|--------|-------------|
| _(vacío)_ | Sin interés | Sin acción |
| `[A]` | **Asistir** | El badge enviará una notificación a tu app Meshtastic 10 minutos antes de que comience |
| `[T]` | **Tal vez** | Recordatorio opcional |
| `[S]` | **Saltar** | Marcado para no asistir |

> [!IMPORTANT]
> Los marcadores se guardan automáticamente en la flash del dispositivo y **persisten entre reinicios**. Si tienes el badge conectado a la app Meshtastic y una plática marcada como `[A]` (Asistir) está por comenzar, recibirás una notificación push en tu teléfono con el mensaje _"Tropicon: [nombre de plática] empieza en 10 min"_.

---

## Monitor AIS de Embarcaciones

La pantalla **AIS** recibe y decodifica señales VHF del sistema de identificación automática de embarcaciones (AIS), usado internacionalmente por barcos para reportar su posición y datos.

Para acceder, navega con **UP/DOWN** hasta la pantalla **AIS**.

### Información mostrada

La pantalla se divide en:

- **Encabezado**: muestra el canal AIS activo (`ch A` o `ch B`, es decir canal 87B o 88B) y el total de tramas recibidas (`frm`).
- **Lista de embarcaciones** (ordenadas de más reciente a más antigua): cada fila contiene:
  - **MMSI**: identificador único de la embarcación (9 dígitos)
  - **RSSI**: nivel de señal recibida en dBm
  - **Antigüedad**: tiempo desde la última trama recibida (en segundos, o `>1h`)
  - **Tipo de mensaje**: badge `T1`, `T2`, etc. (tipo de trama AIS)
  - **Sentencia NMEA**: frase AIS cruda recibida (truncada para caber en pantalla)

Cuando no se han recibido embarcaciones, la pantalla muestra `Listening...` indicando que el receptor está activo y esperando señales.

> [!NOTE]
> El AIS trabaja en frecuencias VHF (~162 MHz). La recepción depende de la proximidad a cuerpos de agua navegables y de la línea de visión con las embarcaciones.

---

## Mensajes — Recibir y Enviar

La pantalla **Messages** muestra todos los mensajes de texto intercambiados en la malla LoRa, presentados en formato de burbuja (mensajes propios a la derecha, mensajes recibidos a la izquierda).

### Navegar a la pantalla de mensajes

Usa **RIGHT/LEFT** desde cualquier pantalla hasta llegar a **Messages**.

### Ver mensajes

- Cada mensaje muestra: remitente, tiempo transcurrido, canal o DM, y el texto.
- Los mensajes propios muestran un indicador de estado de entrega:
  - Palomita `✓` — mensaje confirmado (ACK del destino)
  - `✗` — falla o tiempo de espera agotado
  - Círculo — reenviado por un nodo relay

### Enviar un mensaje (nuevo)

1. Navega a la pantalla **Messages**.
2. Presiona **UP largo** para abrir el menú *"Message Action"*.
3. Selecciona **Reply** → **With Preset** para elegir un mensaje predefinido de la lista.
4. Usa **RIGHT/LEFT** para seleccionar el mensaje deseado y **UP largo** para enviarlo.

> [!NOTE]
> La opción **With Freetext** (teclado libre) solo aparece si se detecta un teclado externo conectado al badge.

### Responder a un mensaje recibido

1. Navega a la pantalla **Messages** donde aparece el mensaje.
2. Presiona **UP largo** para abrir *"Message Action"*.
3. Selecciona **Reply** y elige:
   - **With Preset** — selecciona una respuesta de la lista de mensajes predefinidos.
   - **With Freetext** — escribe tu propio texto (requiere teclado externo).

### Otras opciones del menú Message Action

| Opción | Qué hace |
|--------|----------|
| **Back** | Cierra el menú |
| **Reply** | Abre el submenú para responder |
| **View Chats** | Cambia el filtro de vista: todos los mensajes, por canal o por conversación directa |
| **Mute Channel** / **Unmute Channel** | Silencia o reactiva las notificaciones del canal actual |
| **Delete** | Elimina mensajes (el más antiguo, el actual o todos) |

---

## Telemetría del Entorno

La pantalla **Telemetry** muestra las métricas ambientales medidas en tiempo real por el sensor **AHT10** integrado en el badge:

- **Temperatura** en grados Celsius
- **Humedad relativa** en porcentaje (%)

Para acceder, navega con **UP/DOWN** hasta llegar a la pantalla **Telemetry**.

Las métricas se actualizan automáticamente cada segundo. El badge también transmite estos datos periódicamente por la malla LoRa para que otros nodos y la app Meshtastic puedan visualizarlos.

---

## Conexión por Bluetooth

El badge se conecta con la app oficial de Meshtastic (disponible para Android e iOS) mediante BLE.

### Pasos para emparejar

1. Abre la app **Meshtastic** en tu teléfono.
   ![Home](/imgREDME/HomeMesh.jpg)
2. En la barra inferior, seleccionar el ícono de conexión.
   ![Conexion](/imgREDME/BluetoothMesh.jpg)
3. Iniciar una búsqueda de dispositivos BLE.
   ![Busqueda](/imgREDME/SearchBluetoothMesh.jpg)
4. Selecciona tu badge de la lista. El nombre del dispositivo aparece en la pantalla **Home** del badge.
5. La app solicitará un PIN de 6 dígitos.
6. El badge mostrará en pantalla el mensaje **"Bluetooth — Enter this code"** junto al PIN generado.
7. Ingresa el PIN en la app para completar el emparejamiento.

> [!NOTE]
> El PIN puede ser **aleatorio** (cambia en cada emparejamiento) o **fijo**, según la configuración del dispositivo. El modo por defecto es PIN fijo.

Una vez conectado, la app permite enviar mensajes, ver nodos en el mapa, configurar canales LoRa y recibir notificaciones de la agenda de pláticas.

---

## Set up the Build Environment
1. Instalar [Git](https://git-scm.com/install/)
2. Instalar [Platformio](https://platformio.org/platformio-ide)
3. Clonar el repositorio
   ```bash
   git clone https://github.com/ElectronicCats/Tropicon-2026_Firmware.git
   ```
4. Actualiza los submodulos del repositorio
   ```bash
   cd Tropicon-2026_Firmware && git submodule update --init
   ```

---

## Flasheo del dispositivo

Para cargar el firmware y los datos en tu badge Tropicon 2026, utiliza PlatformIO. El proceso consta de **dos pasos críticos** que deben ejecutarse en este orden:

### Paso 1 — Subir el sistema de archivos (SPIFFS/LittleFS)

Este paso sube la agenda de pláticas, talleres e imágenes de los ponentes al almacenamiento interno del badge. Sin este paso, el dispositivo mostrará `"No talks found"` en la pantalla de agenda.

```bash
pio run -e tropicon2026 -t uploadfs
```

### Paso 2 — Subir el firmware

Una vez que los archivos de datos están en el dispositivo, sube el código principal:

```bash
pio run -e tropicon2026 -t upload
```

> [!IMPORTANT]
> Ambos pasos deben ejecutarse **en ese orden** cada vez que se flashea el dispositivo. Si se omite el Paso 1, el dispositivo arrancará sin los datos de la agenda.

---

## Estructura del proyecto

```
data/
├── schedule.json       # Agenda de pláticas principales (Track 1 y Track Villas)
├── talleres.json       # Agenda de talleres (4 salones)
└── img/                # Imágenes de ponentes (.png) e íconos (.bmp)

src/
├── modules/
│   └── TalksModule.cpp     # Carga de agenda, navegación, marcadores e interés y notificaciones BLE
└── graphics/
    └── draw/
        ├── TalksRenderer.cpp   # Renderizado de lista y detalle de pláticas
        ├── AISRenderer.cpp     # Renderizado de la pantalla de embarcaciones AIS
        ├── MessageRenderer.cpp # Renderizado de mensajes con burbujas
        └── MenuHandler.cpp     # Integración con los menús de Meshtastic
```

---

## Personalizar y modificar el código

El firmware está basado en [Meshtastic](https://meshtastic.org/) con extensiones propias de Electronic Cats. Si después del evento quieres explorar, adaptar o agregar funcionalidades a tu badge, esta guía te da el punto de partida.

### Requisitos previos

- Tener el entorno de compilación configurado ([ver Set up the Build Environment](#set-up-the-build-environment)).
- Conocimientos básicos de C++ y Arduino/ESP32 son útiles, aunque no indispensables para cambios sencillos.
- Familiarizarse con PlatformIO y el archivo `platformio.ini` del repositorio.

### Cambios comunes y dónde hacerlos

#### Modificar la agenda de pláticas

Los datos de la agenda no requieren recompilar el firmware — están en archivos JSON dentro de `data/`:

| Archivo | Qué controla |
|---------|--------------|
| `data/schedule.json` | Pláticas de Track 1 y Track Villas |
| `data/talleres.json` | Talleres de los 4 salones |

Edita los archivos JSON con tu editor favorito y vuelve a subir solo el sistema de archivos:

```bash
pio run -e tropicon2026 -t uploadfs
```

No es necesario recompilar ni re-flashear el firmware completo.

#### Agregar o cambiar imágenes de ponentes

Las imágenes se encuentran en `data/img/` en formato `.bmp`. Reemplaza o agrega archivos con el mismo nombre que referencia el campo `"ImagenTrack1"` o `"ImagenVillas"` en el JSON, y vuelve a correr `uploadfs`.

#### Modificar la lógica de la agenda

El módulo central está en [src/modules/TalksModule.cpp](src/modules/TalksModule.cpp). Ahí se controla:
- Carga de los JSON desde la flash.
- Lógica de navegación (días, stages, pláticas).
- Persistencia de marcadores de interés (`/talks_int.dat`).
- Envío de notificaciones BLE 10 minutos antes de una plática marcada como "Asistir".

#### Modificar cómo se ve la agenda en pantalla

El renderizado visual está en [src/graphics/draw/TalksRenderer.cpp](src/graphics/draw/TalksRenderer.cpp): fuentes, posiciones, íconos y el diseño de la vista lista y detalle.

#### Modificar la pantalla AIS

El renderizado del monitor de embarcaciones está en [src/graphics/draw/AISRenderer.cpp](src/graphics/draw/AISRenderer.cpp). La lógica de recepción y decodificación de tramas AIS está en `src/modules/AIS/`.

#### Agregar nuevas opciones al menú

Los menús emergentes (banners) se definen en [src/graphics/draw/MenuHandler.cpp](src/graphics/draw/MenuHandler.cpp). Busca la función `homeBaseMenu()` para el menú principal o `messageResponseMenu()` para el menú de mensajes y agrega entradas nuevas siguiendo el mismo patrón `enum` + `optionsArray`.

### Flujo de trabajo recomendado

```
1. Editar el archivo fuente correspondiente
2. pio run -e tropicon2026          # Compilar (verifica errores)
3. pio run -e tropicon2026 -t uploadfs   # Si cambiaste data/
4. pio run -e tropicon2026 -t upload    # Subir firmware al badge
```

> [!TIP]
> Si solo modificaste archivos en `data/` (JSON o imágenes), no necesitas recompilar el firmware — basta con el paso `uploadfs`.

### Recursos útiles

- [Documentación oficial de Meshtastic](https://meshtastic.org/docs/) — arquitectura, módulos, protocolo LoRa y API.
- [Repositorio base de Meshtastic firmware](https://github.com/meshtastic/firmware) — código fuente upstream del que se deriva este firmware.
- [Documentación de PlatformIO](https://docs.platformio.org/) — comandos de compilación, ambientes y gestión de librerías.
- [Foro de la comunidad Meshtastic](https://meshtastic.discourse.group/) — preguntas, proyectos y soporte comunitario.
- [Electronic Cats en GitHub](https://github.com/ElectronicCats) — otros proyectos de hardware y firmware de Electronic Cats.
