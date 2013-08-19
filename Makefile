# GNU Make solution makefile autogenerated by Premake
# Type "make help" for usage help

ifndef config
  config=debug
endif
export config

PROJECTS := homegear

.PHONY: all clean help $(PROJECTS)

all: $(PROJECTS)

homegear: 
	@echo "==== Building homegear ($(config)) ===="
	@${MAKE} --no-print-directory -C . -f homegear.make

clean:
	@${MAKE} --no-print-directory -C . -f homegear.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "   debug"
	@echo "   release"
	@echo "   profiling"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   homegear"
	@echo ""
	@echo "For more information, see http://industriousone.com/premake/quick-start"
