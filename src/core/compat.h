/*
 * Astra Core (Compatibility library)
 * http://cesbo.com/astra
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ASC_COMPAT_H_
#define _ASC_COMPAT_H_ 1

#ifndef __PRI64_PREFIX
#   if __WORDSIZE == 64 && !defined(__llvm__)
#       define __PRI64_PREFIX "l"
#   else
#       define __PRI64_PREFIX "ll"
#   endif
#endif /* __PRI64_PREFIX */

#ifndef PRId64
#   define PRId64 __PRI64_PREFIX "d"
#endif /* PRId64 */

#ifndef PRIu64
#   define PRIu64 __PRI64_PREFIX "u"
#endif /* PRIu64 */

#ifndef O_BINARY
#   ifdef _O_BINARY
#       define O_BINARY _O_BINARY
#   else
#       define O_BINARY 0
#   endif
#endif /* O_BINARY */

#ifndef EWOULDBLOCK
#   define EWOULDBLOCK EAGAIN
#endif /* EWOULDBLOCK */

#ifndef HAVE_PREAD
ssize_t pread(int fd, void *buffer, size_t size, off_t off);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *str, size_t max);
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *str, size_t max);
#endif

#endif /* _ASC_COMPAT_H_ */
