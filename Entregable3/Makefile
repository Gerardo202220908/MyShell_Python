CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = sim_kernel
SRC = sim_kernel.c

.PHONY: all run plots clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

plots: run
	python3 generar_graficos.py
	python3 generar_resumen.py

clean:
	rm -f $(TARGET) reporte_thrashing.csv reporte_quantum.csv reporte_resumen.csv traza_*.txt grafico_*.png