CMAKE ?= cmake
BUILD_TYPE ?= Release
JOBS ?= 4
PORT ?= 8080

.PHONY: all native web test sanitize serve clean
all: native

native:
	$(CMAKE) -S . -B build/native -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build/native --parallel $(JOBS)

web:
	emcmake $(CMAKE) -S . -B build/web -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DASHFALL_BUILD_TESTS=OFF
	$(CMAKE) --build build/web --parallel $(JOBS)

test:
	$(CMAKE) -S . -B build/tests -DASHFALL_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build build/tests --parallel $(JOBS)
	ctest --test-dir build/tests --output-on-failure

sanitize:
	$(CMAKE) -S . -B build/sanitize -DASHFALL_BUILD_GAME=OFF -DASHFALL_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build build/sanitize --parallel $(JOBS)
	ctest --test-dir build/sanitize --output-on-failure

serve:
	python3 tools/serve.py --port $(PORT) --directory build/web

clean:
	$(CMAKE) -E rm -rf build
