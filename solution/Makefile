# Define the default target
all: symbolize

# Target to generate the symbolize binary and copy it to the current directory
symbolize:
	@# Run CMake to configure and build the project
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
	cmake --build build
	@# Copy the symbolize binary from the build directory
	cp build/symbolize .

# Target for cleaning up build artifacts
clean:
	rm -rf build symbolize

.PHONY: all symbolize clean
