SRC_DIR := src
TARGET_DIR := target
INCLUDE := include
CC := g++
LIBRARY_PREFIX := lib
DYNAMIC_LIBRARY_SUFFIX := .so

srcs := $(wildcard $(SRC_DIR)/*.cpp)
objs := $(patsubst $(SRC_DIR)/%.cpp,$(TARGET_DIR)/%.o,$(srcs))
apps := $(patsubst $(SRC_DIR)/%.cpp,$(TARGET_DIR)/%,$(srcs))

.PHONY: all clean
all: $(TARGET_DIR)/main

$(TARGET_DIR)/main: $(objs)
	$(CC) -pthread -o $@ $^
%: %.o
	$(CC) -o $@ $<
$(objs): $(TARGET_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) -c -g -fPIC -I $(INCLUDE) -o $@  $<
mpnet: $(TARGET_DIR)/base_socket.o
	$(CC) -shared -o $(TARGET_DIR)/$(LIBRARY_PREFIX)$@$(DYNAMIC_LIBRARY_SUFFIX) $<
main1:
	$(CC) -o target/$@ target/main.o target/service.o -L $(TARGET_DIR) -l mpnet
main2:
	$(CC) -o target/$@ target/main.o -L $(TARGET_DIR) -l mpnet target/service.o
main3:
	$(CC) -o target/$@ target/main.o $(TARGET_DIR)/libmpnet.so target/service.o
main4:
	$(CC) -o target/$@ target/main.o $(TARGET_DIR)/base_socket.o target/service.o	
clean:
	rm -rf $(objs) $(apps)

