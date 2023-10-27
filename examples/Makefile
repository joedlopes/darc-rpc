CXX = g++
CXXFLAGS = -std=c++11 -I../include

BUILD_DIR = build
EXECUTABLES = 01_rpc_server 01_rpc_client 02_rpc_server_single 02_rpc_client_single 03_publisher 03_subscriber 04_tcp_client 04_tcp_server
EXECUTABLES := $(addprefix $(BUILD_DIR)/, $(EXECUTABLES))

.PHONY: all clean

all: $(BUILD_DIR) $(EXECUTABLES)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/01_rpc_server: 01_rpc_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/01_rpc_client: 01_rpc_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/02_rpc_server_single: 02_rpc_server_single.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/02_rpc_client_single: 02_rpc_client_single.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/03_publisher: 03_publisher.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/03_subscriber: 03_subscriber.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/04_tcp_client: 04_tcp_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/04_tcp_server: 04_tcp_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	@rm -rf $(BUILD_DIR)