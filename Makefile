SOURCE_DIR=$(PWD)
BUILD_DIR=$(SOURCE_DIR)/build

.PHONY: xx

"":
	if [ -d $(BUILD_DIR) ]; then \
		cd $(BUILD_DIR) && make -j8; \
	else \
		mkdir -p $(BUILD_DIR); \
		ln -sf $(BUILD_DIR)/compile_commands.json; \
		cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(SOURCE_DIR); \
	fi

%:
	if [ -d $(BUILD_DIR) ]; then \
		cd $(BUILD_DIR) && make $@; \
	else \
		mkdir -p $(BUILD_DIR); \
		ln -sf $(BUILD_DIR)/compile_commands.json; \
		cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(SOURCE_DIR); \
	fi
