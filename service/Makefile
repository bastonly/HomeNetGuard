CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Wall -O2
LDFLAGS = -lcurl -lsqlite3 -lhiredis -lpthread -ldl

COMMON_SRCS = src/db.cpp \
       src/utils.cpp \
       src/sha256.cpp \
       src/nft_manager.cpp \
       src/adguard_manager.cpp \
       src/entertainment_manager.cpp \
       src/device_manager.cpp \
       src/license_manager.cpp \
       src/log_monitor.cpp \
       src/scheduler.cpp \
       src/redis_manager.cpp

COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

SRCS = src/pc_service.cpp $(COMMON_SRCS)
OBJS = $(SRCS:.cpp=.o)

TEST_SRCS = src/test_update_grace.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

TARGET = pc_service
TEST_TARGET = test_update_grace

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJS) $(COMMON_OBJS)
	$(CXX) $(TEST_OBJS) $(COMMON_OBJS) -o $(TEST_TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)
