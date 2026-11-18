# Requiere GBDK-2020 instalado y GBDK_HOME apuntando a su carpeta
# https://github.com/gbdk-2020/gbdk-2020/releases

GBDK_HOME ?= /opt/gbdk
LCC = $(GBDK_HOME)/bin/lcc

SRC = src/main.c src/digits.c
ROM = gbclock.gb

all: $(ROM)

$(ROM): $(SRC)
	$(LCC) -Wa-l -Wl-m -Wl-j -o $(ROM) $(SRC)

clean:
	rm -f $(ROM) *.o *.lst *.map *.sym

run: $(ROM)
	@echo "Abrí $(ROM) en tu emulador (mGBA / SameBoy / Emulicious)"

.PHONY: all clean run
