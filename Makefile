all: compile lint run

lint:
	cppcheck src/ --enable=warning,performance,portability,information

compile:
	mkdir -p build
	gcc src/main.c -O2 -o build/main.exe -I include/ -lGL -lglfw -ldl -lm -Wno-unused-parameter -Wall -Wextra -pedantic -g
	cp src/*.glsl build/
	cp src/*.ttf build/

clean:
	rm -rf build

run:
	@echo ""
	./build/main.exe
