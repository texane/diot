##
## Made by texane <texane@gmail.com>
## 
## Started on  Wed Apr 15 22:59:30 2009 texane
## Last update Wed Apr 15 22:59:36 2009 texane
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
CLFLAGS		=	/nologo /W4 /D_WIN32_WINNT=0x501 /D_WIN32 /DDIOT_DEBUG /D_CRT_SECURE_NO_WARNINGS=1 /D_M_IX86=1 /DDIOT_BUILD_DLL
LD		=	@link.exe
LDFLAGS		=	/nologo
LDLIBS		=


# files

DIOT_DLL	=	$(BIN_DIR)\diot.dll
DIOT_LIB	=	$(BIN_DIR)\diot.lib
DIOT_EXP	=	$(BIN_DIR)\diot.exp
DIOT_SRCS	=	$(SRC_DIR)\diot.c
DIOT_OBJS	=	$(DIOT_SRCS:.c=.obj)


# rules

.SUFFIXES	:	.c .h .obj .exe .exp .lib .dll
all		:	$(DIOT_DLL)
$(DIOT_DLL)	:	$(DIOT_OBJS)
			$(LD) $(LDFLAGS) /DLL /OUT:$(DIOT_DLL) /IMPLIB:$(DIOT_LIB) $(DIOT_OBJS)
.c.obj		:
			$(CL) $(CLFLAGS) /Fo$@ /c /Tc $<
force		:
clean		:	force
			$(RM) $(DIOT_OBJS)
fclean		:	clean
			$(RM) $(DIOT_DLL)
			$(RM) $(DIOT_LIB)
			$(RM) $(DIOT_EXP)
re		:	fclean all
