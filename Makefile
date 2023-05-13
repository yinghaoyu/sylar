.PHONY: xx

xx:
	if [ -d "build" ]; then \
		cd build && make; \
	else \
		mkdir build; \
		cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..; \
	fi

%:
	if [ -d "build" ]; then \
		cd build && make $@; \
	else \
		mkdir build; \
		cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..; \
	fi
