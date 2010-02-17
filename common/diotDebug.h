/*
** Made by texane <texane@gmail.com>
** 
** Started on  Wed Apr 15 22:58:58 2009 texane
** Last update Wed Apr 15 22:58:59 2009 texane
*/



#ifndef DIOT_DEBUG_H_INCLUDED
# define DIOT_DEBUG_H_INCLUDED



#pragma warning(disable: 4127)



#if DIOT_DEBUG

#ifdef DIOT_BUILD_SYS

# include <ntddk.h>

# define DIOT_DEBUG_PRINTF( s, ... ) do { DbgPrint("[?] " __FUNCTION__ ": " s, __VA_ARGS__); } while (0)
# define DIOT_DEBUG_ERROR( s, ... ) do { DbgPrint("[!] " __FUNCTION__ ": " s, __VA_ARGS__); } while (0)
# define DIOT_DEBUG_ENTER() do { DbgPrint("\n[>] " __FUNCTION__ "\n"); } while (0)
# define DIOT_DEBUG_LEAVE() do { DbgPrint("[<] " __FUNCTION__ "\n"); } while (0)

#else /* ! DIOT_BUILD_SYS */

# include <windows.h>
# include <stdio.h>

# define DIOT_DEBUG_PRINTF(s, ...) do { char __b[256]; _snprintf(__b, sizeof(__b) - 1, "[?] " __FUNCTION__ ": " s, __VA_ARGS__); __b[sizeof(__b) - 1] = 0; OutputDebugString(__b); } while (0)
# define DIOT_DEBUG_ERROR(s, ...) do { char __b[256]; _snprintf(__b, sizeof(__b) - 1, "[!] " __FUNCTION__ ": " s, __VA_ARGS__); __b[sizeof(__b) - 1] = 0; OutputDebugString(__b); } while (0)
# define DIOT_DEBUG_ENTER() do { OutputDebugString("[>] " __FUNCTION__ "\n"); } while (0)
# define DIOT_DEBUG_LEAVE() do { OutputDebugString("[<] " __FUNCTION__ "\n"); } while (0)

#endif /* DIOT_BUILD_SYS */

#else /* ! DIOT_DEBUG */

# define DIOT_DEBUG_PRINTF(fmt, ...) do {} while (0)
# define DIOT_DEBUG_ERROR(s, ...) do {} while (0)
# define DIOT_DEBUG_ENTER() do {} while (0)
# define DIOT_DEBUG_LEAVE() do {} while (0)

#endif /* DIOT_DEBUG */



#endif /* ! DIOT_DEBUG_H_INCLUDED */
