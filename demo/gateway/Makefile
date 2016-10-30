#Makefile to build BACnet Gateway Demonstration Application for the Linux Port

# tools - only if you need them.
# Most platforms have this already defined
# CC = gcc

# Executable file name
TARGET = bacgateway

TARGET_BIN = ${TARGET}$(TARGET_EXT)

SRC = main.c \
	$(BACNET_OBJECT)/gw_device.c \
	$(BACNET_HANDLER)/h_routed_npdu.c \
	$(BACNET_HANDLER)/s_router.c \
	$(BACNET_OBJECT)/device.c

# put any overloaded or special built src files here,
# so the linker uses these instead of the functions in the library
OBJECT_SRC =
ifneq (,$(findstring -DAI,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/ai.c
endif
ifneq (,$(findstring -DAO,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/ao.c
endif
ifneq (,$(findstring -DAV,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/av.c
endif
ifneq (,$(findstring -DBI,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/bi.c
endif
ifneq (,$(findstring -DBO,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/bo.c
endif
ifneq (,$(findstring -DBV,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/bv.c
endif
ifneq (,$(findstring -DCHANNEL,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/channel.c
endif
ifneq (,$(findstring -DCOMMAND,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/command.c
endif
ifneq (,$(findstring -DCSV,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/csv.c
endif
ifneq (,$(findstring -DIV,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/iv.c
endif
ifneq (,$(findstring -DLC,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/lc.c
endif
ifneq (,$(findstring -DLO,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/lo.c
endif
ifneq (,$(findstring -DLSP,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/lsp.c
endif
ifneq (,$(findstring -DMSI,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/msi.c
endif
ifneq (,$(findstring -DMSO,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/mso.c
endif
ifneq (,$(findstring -DMSV,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/msv.c
endif
ifneq (,$(findstring -DOSV,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/osv.c
endif
ifneq (,$(findstring -DPIV,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/piv.c
endif
ifneq (,$(findstring -DINTRINSIC_REPORTING,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/nc.c
endif
ifneq (,$(findstring -DTRENDLOG,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/trendlog.c
endif
ifneq (,$(findstring -DSCHEDULE,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/schedule.c
endif
OBJECT_SRC += $(BACNET_OBJECT)/access_credential.c \
	$(BACNET_OBJECT)/access_door.c \
	$(BACNET_OBJECT)/access_point.c \
	$(BACNET_OBJECT)/access_rights.c \
	$(BACNET_OBJECT)/access_user.c \
	$(BACNET_OBJECT)/access_zone.c \
	$(BACNET_OBJECT)/credential_data_input.c
ifneq (,$(findstring -DBACFILE,$(BACNET_DEFINES)))
OBJECT_SRC += $(BACNET_OBJECT)/bacfile.c
endif

SRCS = ${SRC} ${OBJECT_SRC}

OBJS = ${SRCS:.c=.o}

DEFINES += -DBAC_ROUTING

CFLAGS  = $(WARNINGS) $(DEBUGGING) $(OPTIMIZATION) $(STANDARDS) $(INCLUDES) $(DEFINES)

all: ${BACNET_LIB_TARGET} Makefile ${TARGET_BIN}

${TARGET_BIN}: ${OBJS} Makefile ${BACNET_LIB_TARGET}
	${CC} ${PFLAGS} ${OBJS} ${LFLAGS} -o $@
	size $@
	cp $@ ../../bin

lib: ${BACNET_LIB_TARGET}

${BACNET_LIB_TARGET}:
	( cd ${BACNET_LIB_DIR} ; $(MAKE) clean ; $(MAKE) )

.c.o:
	${CC} -c ${CFLAGS} $*.c -o $@

depend:
	rm -f .depend
	${CC} -MM ${CFLAGS} *.c >> .depend

clean:
	rm -f core ${TARGET_BIN} ${OBJS} ${BACNET_LIB_TARGET} $(TARGET).map

include: .depend
