# Bien dich thu tren PC. Firmware that dung STM32CubeIDE, file nay chi de chay
# test doi chieu C vs Python ma khong can board.
#
#   make test     bien dich va chay test_parity
#   make clean

CC      := gcc
CFLAGS  := -std=c99 -O2 -Wall -Wextra -Wdouble-promotion -I Core/Inc -I test
LDLIBS  := -lm
SRC     := Core/Src/model_params.c Core/Src/har_dsp.c Core/Src/har_features.c \
           Core/Src/har_classifier.c Core/Src/har_pipeline.c Core/Src/fall_detect.c
BIN     := build/test_parity

.PHONY: test clean
test: $(BIN)
	./$(BIN)

$(BIN): test/test_parity.c $(SRC) Core/Inc/*.h test/golden_vectors.h
	@mkdir -p build
	$(CC) $(CFLAGS) test/test_parity.c $(SRC) $(LDLIBS) -o $@

clean:
	rm -rf build
