CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wno-unused-result
LDFLAGS = -lsqlite3 -lpthread
TARGET = pctune_up

all: $(TARGET)

$(TARGET): pctune_up.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) pctune_up.cpp $(LDFLAGS)

debug: CXXFLAGS = -std=c++17 -g -DDEBUG -Wall
debug: $(TARGET)

doctor: CXXFLAGS += -DDOCTOR_AGENT
doctor: $(TARGET)

release: CXXFLAGS = -std=c++17 -O3 -march=native -flto -DNDEBUG -Wall
release: LDFLAGS = -lsqlite3 -lpthread -flto
release: $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	sudo ./$(TARGET) $(ARGS)

run-doctor: $(TARGET)
	sudo ./$(TARGET) --doctor

run-dangerous: $(TARGET)
	sudo ./$(TARGET) --dangerous

run-dry: $(TARGET)
	sudo ./$(TARGET) --dry-run

run-admin: $(TARGET)
	sudo ./$(TARGET) --all-admin

run-compare: $(TARGET)
	sudo ./$(TARGET) --compare

run-report: $(TARGET)
	sudo ./$(TARGET) --html-report

reports-dir:
	mkdir -p reports

.PHONY: all clean run run-doctor run-dangerous run-dry run-admin run-compare run-report reports-dir debug doctor release
