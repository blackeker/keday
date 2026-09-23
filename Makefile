# Keday - Native C++ Desktop Pet
# Makefile for MinGW/LLVM-MinGW

CXX = g++
WINDRES = windres
CXXFLAGS = -O2 -std=c++17 -DUNICODE -D_UNICODE -mwindows
LDFLAGS = -static -lgdiplus -lgdi32 -luser32 -lshell32 -lshlwapi -ladvapi32 -lole32 -lcomctl32 -lwinmm

SRC_DIR = src
BUILD_DIR = build
TARGET = Keday.exe

SOURCES = $(SRC_DIR)/main.cpp $(SRC_DIR)/app.cpp $(SRC_DIR)/audio.cpp $(SRC_DIR)/settings.cpp $(SRC_DIR)/features.cpp $(SRC_DIR)/render.cpp $(SRC_DIR)/neko.cpp $(SRC_DIR)/tray.cpp $(SRC_DIR)/settings_window.cpp
OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/app.o $(BUILD_DIR)/audio.o $(BUILD_DIR)/settings.o $(BUILD_DIR)/features.o $(BUILD_DIR)/render.o $(BUILD_DIR)/neko.o $(BUILD_DIR)/tray.o $(BUILD_DIR)/settings_window.o $(BUILD_DIR)/resources.o

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/resources.o: $(SRC_DIR)/resources.rc
	$(WINDRES) $< $@

clean:
	if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	if exist $(TARGET) del $(TARGET)

.PHONY: all clean
