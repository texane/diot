##
## Made by texane <texane@gmail.com>
## 
## Started on  Wed Apr 15 22:59:49 2009 texane
## Last update Wed Apr 15 22:59:50 2009 texane
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

DIOT_EXE	=	$(BIN_DIR)\diot.exe
DIOT_DLL	=	$(BIN_DIR)\diot.dll
DIOT_LIB	=	$(BIN_DIR)\diot.lib
DIOT_SRCS	=	$(SRC_DIR)\main.c
DIOT_OBJS	=	$(DIOT_SRCS:.c=.obj)


# rules

.SUFFIXES	:	.c .h .obj .exe .dll .lib
all		:	$(DIOT_EXE)

$(DIOT_EXE)	:	$(DIOT_LIB) $(DIOT_OBJS)
			$(LD) $(LDFLAGS) /OUT:$(DIOT_EXE) $(DIOT_OBJS) $(DIOT_LIB)
.c.obj		:
			$(CL) $(CLFLAGS) /Fo$@ /c /Tc $<
force		:
clean		:	force
			$(RM) $(DIOT_OBJS)
fclean		:	clean
			$(RM) $(DIOT_EXE)
re		:	fclean all
