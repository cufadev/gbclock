# gbclock

Reloj flip-clock estilo Fliqlo para Game Boy, hecho en C con [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020).

Cuatro tarjetas grandes (HH:MM) con los dos puntos en el medio. Cuando cambia un dígito,
la mitad de arriba del viejo se pliega hacia abajo y después se despliega la mitad de abajo
del nuevo.

## Controles

| Estado | Botón | Acción |
|---|---|---|
| Poner hora / minutos | Arriba / Abajo | Cambia el valor (parpadea el par que se edita) |
| Poner hora / minutos | A | Confirma y pasa al siguiente (hora → minutos → corre) |
| Corriendo | START | Vuelve a poner la hora |
| Corriendo | B | Suma 1 minuto (para probar el flip sin esperar) |

Sin RTC en el cartucho: al apagar se pierde la hora y arranca en modo "poner hora".

## Compilar

1. Bajar [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020/releases) (`gbdk-linux64.tar.gz`) y descomprimirlo.
2. `make GBDK_HOME=/ruta/a/gbdk` → genera `gbclock.gb`.
3. Abrirlo en un emulador (mGBA, SameBoy, Emulicious) o grabarlo en un cartucho.

## Cómo funciona

- **Tarjetas:** cada una usa 32 tiles de fondo (4×8) que se reescriben en tiempo real
  para animar el giro. El flip son 6 cuadros, uno cada 2 frames.
- **Dígitos:** `src/digits.c` se genera con `tools/gen_digits.py` (necesita Pillow y la
  fuente Liberation Sans Bold). Cada dígito es una tarjeta de 32×64 px con 28 px visibles.
  Para cambiar tipografía, tamaño o márgenes (`INSET`) se edita el script y se corre de nuevo:
  `python3 tools/gen_digits.py`. Con un dígito como argumento (`python3 tools/gen_digits.py 8`)
  muestra una vista previa en ASCII.
- **Tiempo:** la Game Boy hace 59.7275 frames/segundo. Un acumulador en la interrupción de
  VBlank suma 100 por frame y resta 5973 cada segundo, así el reloj no se adelanta como
  pasaría con "60 ticks = 1 segundo".

## Grabar en un cartucho real

Se necesita un cartucho regrabable y un [GBxCart RW](https://www.gbxcart.com/) con el
programa [FlashGBX](https://github.com/lesserkuma/FlashGBX). La ROM es de 32 KB sin mapper;
si el cartucho usa otro mapper hay que ajustar el tipo de cartucho al compilar.

## Ideas

- Hora real con un cartucho MBC3 + RTC (los EverDrive GB X5/X7 lo emulan).
- Variantes para fastidiar: reloj que se atrasa solo, campanadas que piden apretar A a tiempo.

## Licencia

GPL-3.0. Ver [LICENSE](LICENSE).
