BUILD_DIR = build
TARGET = $(BUILD_DIR)/fkwhud

all: $(TARGET)

$(TARGET): CMakeLists.txt fkwhud.c
	cmake -B $(BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

release: $(TARGET)
	strip --strip-unneeded $(TARGET)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run release clean
