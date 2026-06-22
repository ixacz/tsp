CC_LINUX=gcc
CC_WINDOWS=x86_64-mingw32-gcc
CFLAGS=-std=c23 -Wall -Wextra -Werror -pedantic -lm
CDEBUGFLAGS=-ggdb
CRELEASEFLAGS=-O3

BINDIR=bin

SRC_INPUT_FILE=./tsp.c

EXE_NAME_LINUX=tsp-linux
EXE_NAME_WINDOWS=tsp-windows

PDF_INPUT_FILE=./travelling-salesman-probem.tex
PDF_OUTPUT_FILE=./travelling-salesman-probem.pdf

ZIP_OUTPUT_FILE=travelling-salesman-probem.zip

TSP_FILES=./syria40.tsp ./kroA100.tsp 
OPT_TOUR_FILES=./kroA100.opt.tour

.PHONY: all
all: build 

.PHONY: build
build: $(EXE_NAME_LINUX) $(EXE_NAME_WINDOWS)

$(EXE_NAME_LINUX): build-linux-debug build-linux-release

build-linux-debug: $(SRC_INPUT_FILE) | $(BINDIR)
	@echo "Building Debugging executable $(EXE_NAME_LINUX)..."
	@$(CC_LINUX) $(SRC_INPUT_FILE) $(CFLAGS) $(CDEBUGFLAGS) -o $(BINDIR)/$(EXE_NAME_LINUX)-debug

build-linux-release: $(SRC_INPUT_FILE) | $(BINDIR)
	@echo "Building Release executable $(EXE)..."
	@$(CC_LINUX) $(SRC_INPUT_FILE) $(CFLAGS) $(CRELEASEFLAGS) -o $(BINDIR)/$(EXE_NAME_LINUX)-release

$(EXE_NAME_WINDOWS): build-windows-debug build-windows-release

build-windows-debug: $(SRC_INPUT_FILE) | $(BINDIR)
	@echo "Building Debugging executable $(EXE_NAME_WINDOWS)..."
	@$(CC_WINDOWS) $(SRC_INPUT_FILE) $(CFLAGS) $(CDEBUGFLAGS) -o $(BINDIR)/$(EXE_NAME_WINDOWS)-debug

build-windows-release: $(SRC_INPUT_FILE) | $(BINDIR)
	@echo "Building Release executable $(EXE_NAME_WINDOWS)..."
	@$(CC_WINDOWS) $(SRC_INPUT_FILE) $(CFLAGS) $(CRELEASEFLAGS) -o $(BINDIR)/$(EXE_NAME_WINDOWS)-release


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

build-dist: $(ZIP_OUTPUT_FILE) build latex  

$(ZIP_OUTPUT_FILE):
	@echo "Zipping the distribution..."
	@zip -r $(ZIP_OUTPUT_FILE) $(BINDIR) $(PDF_OUTPUT_FILE) $(SRC_INPUT_FILE) README.md \
		$(TSP_FILES) $(OPT_TOUR_FILES)

