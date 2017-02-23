/*
 * Astra Unit Tests
 * http://cesbo.com/astra
 *
 * Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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

#include "../libastra.h"

#ifdef _WIN32
#   include <unknwn.h>
#else
#   include <arpa/inet.h>
#   include <netinet/in.h>
#endif

/* C99 printf() format */
START_TEST(c99_printf)
{
    const int d = -1;
    const unsigned int u = 1;
    const long ld = -1;
    const unsigned long lu = 1;
    const long long lld = -1;
    const unsigned long long llu = 1;
    const ssize_t zd = -1;
    const size_t zu = 1;
    const off_t jd = -1;
    const ptrdiff_t td = -2;

    char buf[512] = { 0 };
    const int ret = snprintf(buf, sizeof(buf)
                             , "%d %u %ld %lu %lld %llu %zd %zu %jd %td"
                             , d, u, ld, lu, lld, llu, zd, zu, jd, td);

    static const char expect[] = "-1 1 -1 1 -1 1 -1 1 -1 -2";
    ck_assert(ret == sizeof(expect) - 1);
    ck_assert(strlen(buf) == strlen(expect));
    ck_assert(strcmp(buf, expect) == 0);
}
END_TEST

/* socket() and accept() wrappers */
static inline
void sock_close(int s)
{
#ifdef _WIN32
    ck_assert(closesocket(s) == 0);
#else
    ck_assert(close(s) == 0);
#endif
}

START_TEST(wrap_socket_accept)
{
#ifdef _WIN32
    WSADATA wsaData;
    ck_assert(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    ck_assert(listener != -1 && !is_fd_inherited(listener));

    union
    {
        struct sockaddr_in in;
        struct sockaddr addr;
    } sa;

    memset(&sa, 0, sizeof(sa));
    sa.in.sin_family = AF_INET;
    sa.in.sin_port = 0;
    sa.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    socklen_t addrlen = sizeof(sa);
    ck_assert(bind(listener, &sa.addr, addrlen) == 0);
    ck_assert(getsockname(listener, &sa.addr, &addrlen) == 0);
    ck_assert(listen(listener, SOMAXCONN) == 0);

    const int client = socket(AF_INET, SOCK_STREAM, 0);
    ck_assert(client != -1 && !is_fd_inherited(client));
    ck_assert(connect(client, &sa.addr, addrlen) == 0);

    const int server = accept(listener, NULL, NULL);
    ck_assert(server != -1 && !is_fd_inherited(server));

    sock_close(server);
    sock_close(client);
    sock_close(listener);

#ifdef _WIN32
    WSACleanup();
#endif
}
END_TEST

/* mkstemp() wrapper */
START_TEST(wrap_mkstemp)
{
    char tpl[] = "./test.XXXXXX";

    const int fd = mkstemp(tpl);
    ck_assert(fd != -1 && access(tpl, R_OK) == 0);

#ifdef _WIN32
    /* check handle inheritability */
    const intptr_t osfh = _get_osfhandle(fd);
    ck_assert(osfh != -1 && !is_fd_inherited(osfh));

    /* check translation mode */
    ck_assert(_setmode(fd, _O_BINARY) == _O_BINARY);
#else
    ck_assert(!is_fd_inherited(fd));
#endif

    ck_assert(close(fd) == 0);
    ck_assert(unlink(tpl) == 0);
}
END_TEST

/* open() wrapper */
START_TEST(wrap_open)
{
    char buf[512] = { '\0' };
    ck_assert(snprintf(buf, sizeof(buf), "./test.%d", rand()) > 0);

    const int flags = O_WRONLY | O_CREAT | O_TRUNC;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    const int fd = open(buf, flags, mode);
    ck_assert(fd != -1 && access(buf, R_OK) == 0);

#ifdef _WIN32
    /* check handle inheritability */
    const intptr_t osfh = _get_osfhandle(fd);
    ck_assert(osfh != -1 && !is_fd_inherited(osfh));

    /* check translation mode */
    ck_assert(_setmode(fd, _O_BINARY) == _O_BINARY);
#else
    ck_assert(!is_fd_inherited(fd));
#endif

    ck_assert(close(fd) == 0);
    ck_assert(unlink(buf) == 0);
}
END_TEST

#if defined(HAVE_EPOLL_CREATE) || defined(HAVE_KQUEUE)
/* epoll/kqueue wrappers */
START_TEST(wrap_epoll_kqueue)
{
#ifdef HAVE_EPOLL_CREATE
    const int fd = epoll_create(256);
#else
    const int fd = kqueue();
#endif

    ck_assert(fd != -1 && !is_fd_inherited(fd));
    ck_assert(close(fd) == 0);
}
END_TEST
#endif /* HAVE_EPOLL_CREATE || HAVE_KQUEUE */

#ifdef _WIN32
/* macros specific to Windows builds */
static
ULONG obj_ref;

static STDMETHODCALLTYPE
ULONG obj_Release(IUnknown *obj)
{
    ck_assert(obj != NULL);
    return --obj_ref;
}

START_TEST(win32_macros)
{
    /* ASC_TO_HANDLE: check values up to INT32_MAX */
    static const void *const expect[] =
    {
        (void *)0x0,
        (void *)0x1,
        (void *)0x3,
        (void *)0x7,
        (void *)0xf,
        (void *)0x1f,
        (void *)0x3f,
        (void *)0x7f,
        (void *)0xff,
        (void *)0x1ff,
        (void *)0x3ff,
        (void *)0x7ff,
        (void *)0xfff,
        (void *)0x1fff,
        (void *)0x3fff,
        (void *)0x7fff,
        (void *)0xffff,
        (void *)0x1ffff,
        (void *)0x3ffff,
        (void *)0x7ffff,
        (void *)0xfffff,
        (void *)0x1fffff,
        (void *)0x3fffff,
        (void *)0x7fffff,
        (void *)0xffffff,
        (void *)0x1ffffff,
        (void *)0x3ffffff,
        (void *)0x7ffffff,
        (void *)0xfffffff,
        (void *)0x1fffffff,
        (void *)0x3fffffff,
        (void *)0x7fffffff,
    };

    for (int i = 0; i < (int)ASC_ARRAY_SIZE(expect); i++)
    {
        const int input = (1 << i) - 1;
        ck_assert(ASC_TO_HANDLE(input) == expect[i]);
    }

    /* ASC_RELEASE: safe release macro for cleanup sections */
    obj_ref = 1;

    IUnknownVtbl sample_vtbl =
    {
        .Release = obj_Release,
    };

    IUnknown sample =
    {
        .lpVtbl = &sample_vtbl,
    };

    IUnknown *obj = NULL;
    ASC_RELEASE(obj); /* does nothing */
    ck_assert(obj_ref == 1);
    ck_assert(obj == NULL);

    obj = &sample;
    ASC_RELEASE(obj); /* calls Release(), clears pointer */
    ck_assert(obj_ref == 0);
    ck_assert(sample.lpVtbl == &sample_vtbl);
    ck_assert(sample.lpVtbl->QueryInterface == NULL);
    ck_assert(sample.lpVtbl->AddRef == NULL);
    ck_assert(sample.lpVtbl->Release == obj_Release);
    ck_assert(obj == NULL);

    /* COM NULL check macros */
    typedef struct
    {
        HRESULT hr;
        intptr_t ptr;
        HRESULT want_ptr;
        HRESULT want_enum;
    } want_test_t;

    static const want_test_t want_tests[] =
    {
        { S_OK,    0xdead, S_OK,      S_OK },
        { S_OK,    0x0,    E_POINTER, E_POINTER },

        { S_FALSE, 0xbeef, S_FALSE,   S_FALSE },
        { S_FALSE, 0x0,    E_POINTER, S_FALSE },

        { E_FAIL,  0xcafe, E_FAIL,    E_FAIL },
        { E_FAIL,  0x0,    E_FAIL,    E_FAIL },
    };

    for (size_t i = 0; i < (ASC_ARRAY_SIZE(want_tests) * 2); i++)
    {
        const want_test_t *const test = &want_tests[i / 2];

        HRESULT test_hr = test->hr;
        void *test_ptr = (void *)test->ptr;

        if (i % 2)
        {
            ASC_WANT_ENUM(test_hr, test_ptr);
            ck_assert(test_hr == test->want_enum);
        }
        else
        {
            ASC_WANT_PTR(test_hr, test_ptr);
            ck_assert(test_hr == test->want_ptr);
        }

        ck_assert(test_ptr == (void *)test->ptr);
    }
}
END_TEST

/* functions specific to Windows builds */
START_TEST(win32_funcs)
{
    /* this function is not present on Windows 2000 */
    BOOL in_job = TRUE;
    const BOOL ret = IsProcessInJob(GetCurrentProcess(), NULL, &in_job);
    ck_assert((ret == TRUE && in_job == FALSE) /* XP and later */
              || (ret == FALSE && in_job == TRUE
                  && GetLastError() == ERROR_PROC_NOT_FOUND)); /* 2000 */

    /* Unicode conversion */
    typedef struct
    {
        const wchar_t *wide;
        const char *narrow;
    } uni_test_t;

    static const uni_test_t uni_tests[] =
    {
        { L"", "" },
        { L"Hello World", "Hello World" },
        { L"\a\b\f\n\r\t\v", "\a\b\f\n\r\t\v" },

        /*
         * Unicode test strings.
         *
         * Source: https://www.cl.cam.ac.uk/~mgk25/ucs/examples/quickbrown.txt
         */

        { /* da */
            L"Quizdeltagerne spiste jordbær med fløde, mens cirkusklovnen "
            "Wolther spillede på xylofon",

            "Quizdeltagerne spiste jordbær med fløde, mens cirkusklovnen "
            "Wolther spillede på xylofon",
        },

        { /* de */
            L"Falsches Üben von Xylophonmusik quält jeden größeren Zwerg",

            "Falsches Üben von Xylophonmusik quält jeden größeren Zwerg",
        },

        { /* el */
            L"Γαζέες καὶ μυρτιὲς δὲν θὰ βρῶ πιὰ στὸ χρυσαφὶ ξέφωτο",

            "Γαζέες καὶ μυρτιὲς δὲν θὰ βρῶ πιὰ στὸ χρυσαφὶ ξέφωτο"
        },

        { /* en */
            L"The quick brown fox jumps over the lazy dog",

            "The quick brown fox jumps over the lazy dog",
        },

        { /* es */
            L"El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y "
            "frío, añoraba a su querido cachorro",

            "El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y "
            "frío, añoraba a su querido cachorro",
        },

        { /* fr */
            L"Le cœur déçu mais l'âme plutôt naïve, Louÿs rêva de crapaüter "
            "en canoë au delà des îles, près du mälström où brûlent les novæ",

            "Le cœur déçu mais l'âme plutôt naïve, Louÿs rêva de crapaüter en "
            "canoë au delà des îles, près du mälström où brûlent les novæ",
        },

        { /* ga */
            L"D'fhuascail Íosa, Úrmhac na hÓighe Beannaithe, pór Éava agus "
            "Ádhaimh",

            "D'fhuascail Íosa, Úrmhac na hÓighe Beannaithe, pór Éava agus "
            "Ádhaimh",
        },

        { /* hu */
            L"Árvíztűrő tükörfúrógép",

            "Árvíztűrő tükörfúrógép",
        },

        { /* is */
            L"Kæmi ný öxi hér ykist þjófum nú bæði víl og ádrepa",

            "Kæmi ný öxi hér ykist þjófum nú bæði víl og ádrepa",
        },

        { /* jp */
            L"イロハニホヘト チリヌルヲ ワカヨタレソ ツネナラム "
            "ウヰノオクヤマ ケフコエテ アサキユメミシ ヱヒモセスン",

            "イロハニホヘト チリヌルヲ ワカヨタレソ ツネナラム "
            "ウヰノオクヤマ ケフコエテ アサキユメミシ ヱヒモセスン",
        },

        { /* pl */
            L"Pchnąć w tę łódź jeża lub ośm skrzyń fig",

            "Pchnąć w tę łódź jeża lub ośm skrzyń fig",
        },

        { /* ru */
            L"Съешь же ещё этих мягких французских булок да выпей чаю",

            "Съешь же ещё этих мягких французских булок да выпей чаю",
        },

        { /* th */
            L"เป็นมนุษย์สุดประเสริฐเลิศคุณค่า\n"
            "กว่าบรรดาฝูงสัตว์เดรัจฉาน\n"
            "จงฝ่าฟันพัฒนาวิชาการ\n"
            "อย่าล้างผลาญฤๅเข่นฆ่าบีฑาใคร\n"
            "ไม่ถือโทษโกรธแช่งซัดฮึดฮัดด่า\n"
            "หัดอภัยเหมือนกีฬาอัชฌาสัย\n"
            "ปฏิบัติประพฤติกฎกำหนดใจ\n"
            "พูดจาให้จ๊ะๆ จ๋าๆ น่าฟังเอย ฯ",

            "เป็นมนุษย์สุดประเสริฐเลิศคุณค่า\n"
            "กว่าบรรดาฝูงสัตว์เดรัจฉาน\n"
            "จงฝ่าฟันพัฒนาวิชาการ\n"
            "อย่าล้างผลาญฤๅเข่นฆ่าบีฑาใคร\n"
            "ไม่ถือโทษโกรธแช่งซัดฮึดฮัดด่า\n"
            "หัดอภัยเหมือนกีฬาอัชฌาสัย\n"
            "ปฏิบัติประพฤติกฎกำหนดใจ\n"
            "พูดจาให้จ๊ะๆ จ๋าๆ น่าฟังเอย ฯ",
        },

        { /* tr */
            L"Pijamalı hasta, yağız şoföre çabucak güvendi",

            "Pijamalı hasta, yağız şoföre çabucak güvendi",
        },
    };

    for (size_t i = 0; i < ASC_ARRAY_SIZE(uni_tests); i++)
    {
        const uni_test_t *const test = &uni_tests[i];

        wchar_t *const wide = cx_widen(test->narrow);
        ck_assert(wide != NULL);
        ck_assert(!wcscmp(wide, test->wide));
        free(wide);

        char *const narrow = cx_narrow(test->wide);
        ck_assert(narrow != NULL);
        ck_assert(!strcmp(narrow, test->narrow));
        free(narrow);
    }

    /* wrapper around GetModuleFileName */
    char *const exe = cx_exepath();
    ck_assert(exe != NULL && strlen(exe) > 0);
    ck_assert(access(exe, R_OK) == 0);
    char *const dot = strrchr(exe, '.');
    ck_assert(strcasecmp(dot, ".exe") == 0);
    free(exe);
}
END_TEST
#endif /* _WIN32 */

Suite *core_compat(void)
{
    Suite *const s = suite_create("core/compat");
    TCase *const tc = tcase_create("default");

    tcase_add_test(tc, c99_printf);
    tcase_add_test(tc, wrap_socket_accept);
    tcase_add_test(tc, wrap_mkstemp);
    tcase_add_test(tc, wrap_open);
#if defined(HAVE_EPOLL_CREATE) || defined(HAVE_KQUEUE)
    tcase_add_test(tc, wrap_epoll_kqueue);
#endif
#ifdef _WIN32
    tcase_add_test(tc, win32_macros);
    tcase_add_test(tc, win32_funcs);
#endif

    suite_add_tcase(s, tc);

    return s;
}
