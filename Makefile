all: compile lint run

lint:
	cppcheck . --enable=warning,performance,portability

compile:
	mkdir -p build
	gcc src/main.c src/gl3w.c -O2 -o build/main.exe -I include/ -lGL -lglfw -ldl -lm -Wall -Wextra -pedantic -g

clean:
	rm -rf build

run:
	./build/main.exe
