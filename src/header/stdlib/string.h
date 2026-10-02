#ifndef _STRING_H
#define _STRING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Sets n bytes of memory area to constant byte value.
 * 
 * @param s Pointer to memory area to set
 * @param c Constant byte value for filling memory area
 * @param n Memory area size in byte 
 * 
 * @return Pointer s
*/
void* memset(void *s, int c, size_t n);

/**
 * @brief Copies n bytes from source to destination.
 * 
 * @param dest Starting location for memory area to set
 * @param src Pointer to source memory
 * @param n Memory area size in byte 
 * 
 * @return Pointer dest
*/
void* memcpy(void* restrict dest, const void* restrict src, size_t n);

/**
 * @brief Compares two memory areas byte by byte.
 * 
 * @param s1 Pointer to first memory area
 * @param s2 Pointer to second memory area
 * @param n Memory area size in byte 
 * 
 * @return Integer as error code, zero for equality, non-zero for inequality
*/
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * @brief Copies n bytes from source to destination, handling overlapping regions.
 * 
 * @param dest Pointer to destination memory
 * @param src Pointer to source memory
 * @param n Memory area size in byte 
 * 
 * @return Pointer dest
*/
void *memmove(void *dest, const void *src, size_t n);

/**
 * @brief Compute length of string not including null terminator.
 *
 * @param s String to measure.
 *
 * @return Length of s.
 */
size_t strlen(const char *s);

#endif