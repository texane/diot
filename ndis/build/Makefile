## 
## Made by texane <texane@gmail.com>
## 
## Started on  Mon Apr 20 08:16:26 2009 texane
## Last update Mon Apr 20 08:17:43 2009 texane
##



# dirs

SRC_DIR		=	..\src
BIN_DIR		=	..\..\bin


# tools

RM		=	@del 2>NUL:
CP		=	@copy /Y
MV		=	@move /Y


# compil

CL		=	@cl.exe
CLFLAGS		=	/nologo /W4 /D_WIN32_WINNT=0x501 /D_WIN32 /D_DEBUG=0 /D_CRT_SECURE_NO_WARNINGS=1 /D_M_IX86=1
LD		=	@link.exe
LDFLAGS		=	/nologo
LDLIBS		=


# files

DIOT_DLL	=	$(BIN_DIR)\diot.dll
DIOT_LIB	=	$(BIN_DIR)\diot.lib
NDIS_EXE	=	$(BIN_DIR)\ndis.exe
NDIS_SRCS	=	$(SRC_DIR)\main.c
NDIS_OBJS	=	$(NDIS_SRCS:.c=.obj)


# rules

.SUFFIXES	:	.c .h .obj .exe .dll .lib
all		:	$(NDIS_EXE)

$(NDIS_EXE)	:	$(DIOT_LIB) $(NDIS_OBJS)
			$(LD) $(LDFLAGS) /OUT:$(NDIS_EXE) $(NDIS_OBJS) $(DIOT_LIB)
.c.obj		:
			$(CL) $(CLFLAGS) /Fo$@ /c /Tc $<
force		:
clean		:	force
			$(RM) $(NDIS_OBJS)
fclean		:	clean
			$(RM) $(NDIS_EXE)
re		:	fclean all
