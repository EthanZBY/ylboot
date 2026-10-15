/*
 *  charset conversion utils
 *
 *  Copyright (c) 2017 Rob Clark
 *
 *  SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __CHARSET_H_
#define __CHARSET_H_

#define MAX_UTF8_PER_UTF16 4

/**
 * utf16_strlen() - Get the length of an utf16 string
 *
 * Returns the number of 16 bit characters in an utf16 string, not
 * including the terminating NULL character.
 *
 * @in     the string to measure
 * @return the string length
 */
size_t utf16_strlen(const uint16_t *in);

/**
 * utf16_strnlen() - Get the length of a fixed-size utf16 string.
 *
 * Returns the number of 16 bit characters in an utf16 string,
 * not including the terminating NULL character, but at most
 * 'count' number of characters.  In doing this, utf16_strnlen()
 * looks at only the first 'count' characters.
 *
 * @in     the string to measure
 * @count  the maximum number of characters to count
 * @return the string length, up to a maximum of 'count'
 */
size_t utf16_strnlen(const uint16_t *in, size_t count);

/**
 * utf16_strcpy() - UTF16 equivalent of strcpy()
uint16_t *utf16_strcpy(uint16_t *dest, const uint16_t *src);

/**
 * utf16_strdup() - UTF16 equivalent of strdup()
 */
uint16_t *utf16_strdup(const uint16_t *s);

/**
 * utf16_to_utf8() - Convert an utf16 string to utf8
 *
 * Converts 'size' characters of the utf16 string 'src' to utf8
 * written to the 'dest' buffer.
 *
 * NOTE that a single utf16 character can generate up to 4 utf8
 * characters.  See MAX_UTF8_PER_UTF16.
 *
 * @dest   the destination buffer to write the utf8 characters
 * @src    the source utf16 string
 * @size   the number of utf16 characters to convert
 * @return the pointer to the first unwritten byte in 'dest'
 */
uint8_t *utf16_to_utf8(uint8_t *dest, const uint16_t *src, size_t size);
/**
 * utf8_to_utf32_stream() - convert UTF-8 byte stream to Unicode code points
 *
 * The function is called for each byte @c in a UTF-8 stream. The byte is
 * appended to the temporary storage @buffer until the UTF-8 stream in
 * @buffer describes a Unicode code point.
 *
 * When a new code point has been decoded it is returned and buffer[0] is
 * set to '\0', otherwise the return value is 0.
 *
 * The buffer must be at least 5 characters long. Before the first function
 * invocation buffer[0] must be set to '\0'."
 *
 * @c:		next UTF-8 character to convert
 * @buffer:	buffer, at least 5 characters
 * Return:	Unicode code point or 0
 */
int utf8_to_utf32_stream(u8 c, char *buffer);
/**
 * u16_strncmp() - compare two u16 string
 *
 * @s1:		first string to compare
 * @s2:		second string to compare
 * @n:		maximum number of u16 to compare
 * Return:	0  if the first n u16 are the same in s1 and s2
 *		< 0 if the first different u16 in s1 is less than the
 *		corresponding u16 in s2
 *		> 0 if the first different u16 in s1 is greater than the
 *		corresponding u16 in s2
 */
int u16_strncmp(const u16 *s1, const u16 *s2, size_t n);
/**
 * u16_strcmp() - compare two u16 string
 *
 * @s1:		first string to compare
 * @s2:		second string to compare
 * Return:	0  if the first n u16 are the same in s1 and s2
 *		< 0 if the first different u16 in s1 is less than the
 *		corresponding u16 in s2
 *		> 0 if the first different u16 in s1 is greater than the
 *		corresponding u16 in s2
 */
#define u16_strcmp(s1, s2)	u16_strncmp((s1), (s2), SIZE_MAX)

//jc 7.21
int u16_strncmp(const u16 *s1, const u16 *s2, size_t n);
#define u16_strlen utf16_strlen

#endif /* __CHARSET_H_ */
