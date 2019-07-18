
all: bleconf

CPPFLAGS=-Wall -Wextra -I$(HOSTAPD_HOME)/src/utils -I$(HOSTAPD_HOME)/src/common -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX
CPPFLAGS+=-I$(CJSON_HOME)
CPPFLAGS+=-I$(BLUEZ_HOME)
CPPFLAGS+=$(shell pkg-config --cflags glib-2.0) -g
LDFLAGS+=$(shell pkg-config --libs glib-2.0)
LDFLAGS+=-pthread -L$(CJSON_HOME) -lcjson -lcrypto

WITH_BLUEZ=1
PLATFORM = "RASPBERRYPI"

SRCS=\
  gattdata.cc \
  jsonwrapper.cc \
  main.cc \
  logger.cc \
  rpcserver.cc \
  util.cc

ifeq ($(PLATFORM), "RASPBERRYPI")
  CPPFLAGS+=-DPLATFORM_RASPBERRYPI
  SRCS += gattdata_pi.cc
endif

ifeq ($(WITH_BLUEZ), 1)
  CPPFLAGS+=-DWITH_BLUEZ
  BLUEZ_LIBS+=-L$(BLUEZ_HOME)/src/.libs/ -lshared-mainloop -L$(BLUEZ_HOME)/lib/.libs -lbluetooth-internal
  SRCS+=gattserver.cc
  SRCS+=beacon.cc
endif

OBJS=$(patsubst %.cc, %.o, $(notdir $(SRCS)))

clean:
	$(RM) -f $(OBJS) bleconf

bleconf: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o bleconf $(BLUEZ_LIBS)

gattserver.o: bluez/gattserver.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

beacon.o: bluez/beacon.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

gattdata_pi.o: pi/gattdata_pi.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

