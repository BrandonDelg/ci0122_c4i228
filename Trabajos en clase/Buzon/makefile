
# Directorios
SRC=src
BUILD=build
BIN=bin

# Compilador y flags
CXX=g++
CC=gcc
STD=-std=c++20
WFLAGS=-Wall -Wextra -g

PROGRAMS = enviarConClases recibirConClases enviarSinClases recibirSinClases

# Archivos fuente y objetos
DIRS=$(shell find -L $(SRC) -type d)
FSOURCE=$(wildcard $(DIRS:%=%/*.cpp))
FHEADERS=$(wildcard $(DIRS:%=%/*.hpp))
FOBJECT=$(FSOURCE:$(SRC)/%.cpp=$(BUILD)/%.o)

.PHONY: all build clean run show debug lint lint-file

# Compila todos los programas
all: $(PROGRAMS:%=$(BIN)/%)

# Regla genérica: linkea binario desde objetos
$(BIN)/%: $(BUILD)/%.o $(BUILD)/Buzon.o | $(BIN)
	$(CXX) $(STD) $(WFLAGS) $^ -o $@

# Compila objetos individuales (C++)
$(BUILD)/%.o: $(SRC)/%.cpp | $(BUILD)
	$(CXX) $(STD) $(WFLAGS) -Isrc -c $< -o $@

# Crear carpetas
$(BIN):
	mkdir -p $@

$(BUILD):
	mkdir -p $@

# Ejecutar binario específico
run-%: $(BIN)/%
	./$<

# Limpiar
clean:
	rm -rf $(BIN) $(BUILD)

# Mostrar objetos
show:
	echo $(FOBJECT)

# Depuración
debug-%: $(BIN)/%
	gdb ./$<

# Lint global
lint:
	@echo "Ejecutando cpplint..."
	@find $(SRC) -name "*.cpp" -o -name "*.hpp" | xargs cpplint $(CPPLINT_FLAGS)
	@echo "Análisis completado"

# Lint de archivo específico
lint-file:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make lint-file FILE=path/to/file.cpp"; \
		exit 1; \
	fi
	@echo "Analizando $(FILE)..."
	@cpplint $(CPPLINT_FLAGS) $(FILE) || true
	@echo "Análisis completado"
