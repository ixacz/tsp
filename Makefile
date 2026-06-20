CC=gcc
CFLAGS=-std=c23 -Wall -Wextra -Werror -pedantic -lm
CDEBUGFLAGS=-ggdb

BINDIR=bin

SRC_INPUT_FILE=./tsp.c
EXENAME=tsp
EXE=$(BINDIR)/$(EXENAME)

PDF_INPUT_FILE=./travelling-salesman-probem.tex
PDF_OUTPUT_FILE=./travelling-salesman-probem.pdf

.PHONY: all
all: build latex

.PHONY: build
build: $(EXE)

$(EXE): $(SRC_INPUT_FILE) | $(BINDIR)
	@echo "Building executable $(EXE)..."
	@$(CC) $(SRC_INPUT_FILE) $(CFLAGS) $(CDEBUGFLAGS) -o $(EXE)

.PHONY: $(BINDIR)
$(BINDIR):
	@mkdir -p bin

latex: $(PDF_OUTPUT_FILE)

$(PDF_OUTPUT_FILE): $(PDF_INPUT_FILE)
	@echo "Compiling LaTex file..."
	@xelatex $(PDF_INPUT_FILE)

push-pdf-file: $(PDF_OUTPUT_FILE)
	@echo "Pushing pdf file output to the phone"
	@adb push travelling-salesman-probem.pdf /sdcard/Documents/pdfs/tsp

