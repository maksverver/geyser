CXXFLAGS=-Wall -O2 -Iomnithread
CXXFLAGS+=-D__`uname | tr A-Z a-z`__
#CXXFLAGS+=-g -DDEBUG
LDFLAGS=-pthread
LDLIBS=-lcrypto -lpthread
COMMON_OBJECTS=omnithread/omnithread.o \
	MetaInfo.o bcoding.o debug.o paths.o settings.o sha.o
SERVER_OBJECTS=$(COMMON_OBJECTS) HttpRequest.o Socket.o TorrentDirectory.o \
        TorrentPeer.o TorrentSeeder.o TorrentTracker.o main.o
METAINFO_OBJECTS=$(COMMON_OBJECTS) metainfo_main.o

all: geyser

geyser: $(SERVER_OBJECTS)
	$(CXX) $(LDLIBS) $(LDFLAGS) -o geyser $(SERVER_OBJECTS)

metainfo: $(METAINFO_OBJECTS)
	$(CXX) $(LDLIBS) $(LDFLAGS) -o metainfo $(METAINFO_OBJECTS)

omnithread/omnithread.o:
	cd omnithread && CXXFLAGS=-D__`uname | tr A-Z a-z`__ make

clean:
	-rm metainfo
	-rm geyser
	-rm *.o
	cd omnithread && make clean
