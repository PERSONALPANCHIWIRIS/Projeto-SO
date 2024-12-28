# Makefile to delete all .out and .bck files in the jobs directory

# Define the directory containing the job files
JOBS_DIR = .

# Define the file patterns to be deleted
OUT_FILES = $(JOBS_DIR)/*.out
BCK_FILES = $(JOBS_DIR)/*.bck

# Default target
all: clean

# Target to delete .out and .bck files
clean:
	@echo "Deleting .out and .bck files in $(JOBS_DIR)..."
	@rm -f $(OUT_FILES) $(BCK_FILES)
	@echo "Cleanup complete."

.PHONY: all clean