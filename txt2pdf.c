/*
 * txt2pdf THE SPARTAN PDF GENERATOR
 * Copyright (C) 2026  John (johna124)
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Source, issues, contact: https://github.com/johna124
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <locale.h>
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-function"
#endif


/*
 * txt2pdf.c v1.7
 *
 * Simple TXT to PDF converter in pure C.
 *
 * Features:
 * - Generates PDF 1.4 without external libraries.
 * - Uses Courier font.
 * - Creates PDF bookmarks (outline) for numbered sections.
 * - Creates internal links in the TOC.
 *
 * Changes v1.7.1:
 * - Improved winansi char detection
 *
 * Changes v1.7:
 * - Added --sans, --serif and --mono options for Base14 fonts.
 * - Added [IMG]file.jpg[/IMG] and [IMG]file.png[/IMG] block images.
 * - JPEG images are embedded with native /DCTDecode passthrough.
 * - Simple PNG images are embedded with /FlateDecode PNG predictor.
 * - Proportional fonts use built-in ASCII metrics for wrapping.
 *  
 * Changes v1.6:
 * - CRITICAL FIX: parse_section_line_dyn() now captures complete
 *   hierarchical IDs (2.3, 11.14, 13.1b, 26.5a, 2.0a, etc.).
 *   Previously only captured the first number ("2" for "2.3 ..."),
 *   which caused all TOC links of subsections to point to the
 *   main section.
 * - Support for subsections of any depth (2.3.1, 2.3.1.2).
 * - Support for alphanumeric suffixes (2.0a, 13.1b, 26.5a).
 * - Correct handling of "2. Title" (loose dot without subsection).
 *
 * Changes v1.5:
 * - Auto-detection of UTF-8 vs Latin-1/ISO-8859-1 encoding.
 * - Smart decoder in pdf_escape to translate UTF-8 to WinAnsiEncoding.
 * - Full support for accents, ñ and umlauts.
 *
 * Compile:
 *   gcc -O2 -Wall -Wextra -std=c11 -o txt2pdf txt2pdf.c
 *
 * Usage:
 *   ./txt2pdf readme.txt readme.pdf
 *   ./txt2pdf --debug readme.txt readme.pdf
 */

#define PAGE_WIDTH   595.0
#define PAGE_HEIGHT  842.0
#define MARGIN       50.0
#define FONT_SIZE    9.0
#define LEADING      12.0
#define CHAR_WIDTH   (FONT_SIZE * 0.6)

/* ========================================================================
 * 1. Header and Detection (UTF-8 vs Latin-1)
 * ======================================================================== */

static int g_is_utf8 = 0;

static int detect_if_utf8(const char *data, size_t len)
{
    int has_multibyte = 0;

    for (size_t i = 0; i < len; ) {
        unsigned char c = (unsigned char)data[i];

        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || ((unsigned char)data[i + 1] & 0xC0) != 0x80)
                return 0;
            has_multibyte = 1;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len
                || ((unsigned char)data[i + 1] & 0xC0) != 0x80
                || ((unsigned char)data[i + 2] & 0xC0) != 0x80)
                return 0;
            has_multibyte = 1;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len
                || ((unsigned char)data[i + 1] & 0xC0) != 0x80
                || ((unsigned char)data[i + 2] & 0xC0) != 0x80
                || ((unsigned char)data[i + 3] & 0xC0) != 0x80)
                return 0;
            has_multibyte = 1;
            i += 4;
        } else {
            return 0;
        }
    }

    return has_multibyte;
}

/* ========================================================================
 * Memory structures and utilities
 * ======================================================================== */

typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} Str;

static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n);
    if (!q) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    return q;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char  *p = xmalloc(n);
    memcpy(p, s, n);
    return p;
}

static char *xstrndup(const char *s, size_t n)
{
    char *p = xmalloc(n + 1);
    if (n > 0)
        memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static void str_reserve(Str *s, size_t need)
{
    if (need == 0)
        need = 1;
    if (s->cap < need) {
        size_t ncap = s->cap ? s->cap : 64;
        while (ncap < need)
            ncap *= 2;
        s->data = xrealloc(s->data, ncap);
        s->cap  = ncap;
    }
    if (s->data && s->len < s->cap)
        s->data[s->len] = '\0';
}

static void str_init(Str *s)
{
    s->data = NULL;
    s->len  = 0;
    s->cap  = 0;
    str_reserve(s, 1);
    s->len = 0;
    s->data[0] = '\0';
}

static void str_free(Str *s)
{
    free(s->data);
    s->data = NULL;
    s->len  = 0;
    s->cap  = 0;
}

static void str_append(Str *s, const char *data, size_t len)
{
    str_reserve(s, s->len + len + 1);
    memcpy(s->data + s->len, data, len);
    s->len += len;
    s->data[s->len] = '\0';
}

static void str_append_str(Str *s, const char *str)
{
    str_append(s, str, strlen(str));
}

static void str_append_char(Str *s, char c)
{
    str_reserve(s, s->len + 2);
    s->data[s->len++] = c;
    s->data[s->len]   = '\0';
}

static void str_appendf(Str *s, const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        return;
    }

    str_reserve(s, s->len + (size_t)n + 1);
    vsnprintf(s->data + s->len, (size_t)n + 1, fmt, ap2);
    s->len += (size_t)n;
    va_end(ap2);
}

/* ========================================================================
 * 2. 'pdf_escape' function (Smart Decoder)
 * ======================================================================== */

static void pdf_escape(const char *s, Str *out)
{
    if (!out->data) {
        out->len = 0;
        out->cap = 0;
        str_init(out);
    } else {
        out->len = 0;
        if (out->cap > 0)
            out->data[0] = '\0';
    }

    const unsigned char *p = (const unsigned char *)s;

    while (*p) {
        unsigned char c = *p;

        /* Dynamic UTF-8 -> WinAnsiEncoding translation */
        if (g_is_utf8 && c >= 0xC2 && c <= 0xDF) {
            unsigned char c2 = p[1];
            if (c2 >= 0x80 && c2 <= 0xBF) {
                int cp = ((c & 0x1F) << 6) | (c2 & 0x3F);
                unsigned char winansi = 0;

               switch (cp) {
                    case 0x00E1: winansi = 0xE1; break; /* á */
                    case 0x00E9: winansi = 0xE9; break; /* é */
                    case 0x00ED: winansi = 0xED; break; /* í */
                    case 0x00F3: winansi = 0xF3; break; /* ó */
                    case 0x00FA: winansi = 0xFA; break; /* ú */
                    case 0x00F1: winansi = 0xF1; break; /* ñ */
                    case 0x00FC: winansi = 0xFC; break; /* ü */
                    case 0x00C1: winansi = 0xC1; break; /* Á */
                    case 0x00C9: winansi = 0xC9; break; /* É */
                    case 0x00CD: winansi = 0xCD; break; /* Í */
                    case 0x00D3: winansi = 0xD3; break; /* Ó */
                    case 0x00DA: winansi = 0xDA; break; /* Ú */
                    case 0x00D1: winansi = 0xD1; break; /* Ñ */
                    case 0x00DC: winansi = 0xDC; break; /* Ü */
                    case 0x00BF: winansi = 0xBF; break; /* ¿ */
                    case 0x00A1: winansi = 0xA1; break; /* ¡ */
                    case 0x2019: winansi = 0x27; break; /* ’ -> ASCII ' */
                    case 0x2018: winansi = 0x27; break; /* ‘ -> ASCII ' */
                    case 0x201C: winansi = 0x22; break; /* “ -> ASCII " */
                    case 0x201D: winansi = 0x22; break; /* ” -> ASCII " */
                    case 0x2014: winansi = 0x97; break; /* Raya larga — -> WinAnsi 0x97 */
                    case 0x2026: winansi = 0x85; break; /* Elipsis … -> WinAnsi 0x85 */
                    case 0x20AC: winansi = 0x88; break; /* Euro € -> WinAnsi 0x88 */
                    default:     winansi = '?';  break;
                }

                str_append_char(out, (char)winansi);
                p += 2;
                continue;
            }
        }

        /* Tu procesamiento ASCII estándar intacto */
        if      (c == '\\') str_append_str(out, "\\\\");
        else if (c == '(')  str_append_str(out, "\\(");
        else if (c == ')')  str_append_str(out, "\\)");
        else if (c == '\r') str_append_str(out, "\\r");
        else if (c == '\n') str_append_str(out, "\n");
        else if (c >= 32)   str_append_char(out, (char)c);
        else if (c == '\t') str_append_char(out, ' ');
        else                str_append_char(out, '?');

        p++;
    }
}

/* ========================================================================
 * Text and calculation utilities
 * ======================================================================== */

static double visible_width(const char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    return (double)n * CHAR_WIDTH;
}

static void next_visual_chunk(const char *s,
                              size_t      off,
                              size_t      slen,
                              int         max_chars,
                              char      **out_text,
                              size_t     *out_consume)
{
    size_t remaining = slen - off;

    if (remaining <= (size_t)max_chars) {
        *out_text    = xstrndup(s + off, remaining);
        *out_consume = remaining;
        return;
    }

    int br = -1;
    for (int k = max_chars - 1; k >= 0; --k) {
        if (isspace((unsigned char)s[off + (size_t)k])) {
            br = k;
            break;
        }
    }

    if (br < 0) {
        for (int k = max_chars - 1; k >= 0; --k) {
            unsigned char c = (unsigned char)s[off + (size_t)k];
            if (c == '-' || c == '/') {
                br = k;
                break;
            }
        }
    }

    if (br >= 0) {
        size_t len = (size_t)br + 1;
        *out_text    = xstrndup(s + off, len);
        *out_consume = len;
        return;
    }

    if (off + (size_t)max_chars < slen
        && isspace((unsigned char)s[off + (size_t)max_chars])) {
        *out_text    = xstrndup(s + off, (size_t)max_chars);
        *out_consume = (size_t)max_chars;
        return;
    }

    if (max_chars > 1) {
        size_t len = (size_t)max_chars - 1;
        char  *buf = xmalloc(len + 2);
        memcpy(buf, s + off, len);
        buf[len]     = '-';
        buf[len + 1] = '\0';
        *out_text    = buf;
        *out_consume = len;
    } else {
        *out_text    = xstrndup(s + off, 1);
        *out_consume = 1;
    }
}

static char *read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }

    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }

    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    char  *buf = xmalloc((size_t)sz + 1);
    size_t rd  = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);

    if (out_len)
        *out_len = rd;

    return buf;
}

static char **split_lines(const char *data, size_t len, int *out_n)
{
    char  **lines = NULL;
    size_t  n = 0, cap = 0;

    if (len == 0) {
        lines = xmalloc(sizeof(char *));
        lines[0] = xstrdup("");
        n = 1;
    } else {
        const char *p   = data;
        const char *end = data + len;

        while (p < end) {
            const char *start = p;
            while (p < end && *p != '\n')
                p++;

            size_t l = (size_t)(p - start);
            if (l > 0 && start[l - 1] == '\r')
                l--;

            char *line = xmalloc(l + 1);
            memcpy(line, start, l);
            line[l] = '\0';

            if (n == cap) {
                cap   = cap ? cap * 2 : 64;
                lines = xrealloc(lines, cap * sizeof(char *));
            }
            lines[n++] = line;

            if (p < end && *p == '\n')
                p++;
        }
    }

    *out_n = (int)n;
    return lines;
}

static char *trim_dup(const char *src)
{
    const char *p = src;
    while (isspace((unsigned char)*p))
        p++;

    size_t len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1]))
        len--;

    return xstrndup(p, len);
}

static int is_separator(const char *line)
{
    int count = 0;
    const char *p = line;

    while (*p) {
        if (*p == '=')
            count++;
        else if (!isspace((unsigned char)*p))
            return 0;
        p++;
    }

    return count >= 10;
}

static int line_is_blank(const char *s)
{
    while (*s) {
        if (!isspace((unsigned char)*s))
            return 0;
        s++;
    }
    return 1;
}

static int nearby_prev_is_separator(char **lines, int nlines, int i)
{
    (void)nlines;
    for (int j = i - 1; j >= 0 && j >= i - 5; --j) {
        if (line_is_blank(lines[j]))
            continue;
        return is_separator(lines[j]);
    }
    return 0;
}

static int nearby_next_is_separator(char **lines, int nlines, int i)
{
    for (int j = i + 1; j < nlines && j <= i + 5; ++j) {
        if (line_is_blank(lines[j]))
            continue;
        return is_separator(lines[j]);
    }
    return 0;
}

/* ========================================================================
 * SECTION PARSER — v1.6: COMPLETE HIERARCHICAL IDs
 * ========================================================================
 *
 * Correctly captures:
 *   "1. Title"                     -> id = "1"
 *   "2.3 Authentication SSH"       -> id = "2.3"
 *   "2.0a Logical order"           -> id = "2.0a"
 *   "11.14 Summary"                -> id = "11.14"
 *   "13.1b Variables"              -> id = "13.1b"
 *   "26.5a Validation"             -> id = "26.5a"
 *   "2.3.1 Deep subsection"        -> id = "2.3.1"
 *
 * BUG v1.5: the previous parser only captured the first number,
 * so "2.3 Authentication" and "2. ARCHITECTURE" both produced
 * id = "2", causing TOC links of subsections to point to the
 * main section.
 */

static int parse_section_line_dyn(const char *line,
                                  char      **id_out,
                                  char      **title_out)
{
    const char *p = line;

    while (isspace((unsigned char)*p))
        p++;

    if (!isdigit((unsigned char)*p))
        return 0;

    Str id;
    str_init(&id);

    /* Read first number */
    while (isdigit((unsigned char)*p)) {
        str_append_char(&id, *p);
        p++;
    }

    /* Allow letters attached to the first number (e.g. "2a", "13b") */
    while (isalpha((unsigned char)*p)) {
        str_append_char(&id, (char)tolower((unsigned char)*p));
        p++;
    }

    /* Read hierarchical subsections: ".N", ".Na", ".N.M", etc. */
    while (*p == '.') {
        const char *save_p = p;
        p++;  /* skip the dot tentatively */

        /* Must have at least one digit after the dot to be a valid
         * subsection. Otherwise, rollback (it was a loose dot). */
        if (!isdigit((unsigned char)*p)) {
            p = save_p;
            break;
        }

        str_append_char(&id, '.');

        while (isdigit((unsigned char)*p)) {
            str_append_char(&id, *p);
            p++;
        }

        /* Allow letters at the end (e.g. "2.0a", "13.1b", "26.5a") */
        while (isalpha((unsigned char)*p)) {
            str_append_char(&id, (char)tolower((unsigned char)*p));
            p++;
        }
    }

    /* Consume a loose "." if remaining (e.g. "2. Title" without subsection) */
    if (*p == '.')
        p++;

    /* Must have a space or end of line */
    if (*p != '\0' && !isspace((unsigned char)*p)) {
        str_free(&id);
        return 0;
    }

    while (isspace((unsigned char)*p))
        p++;

    char *title = trim_dup(p);

    if (id.len == 0) {
        str_free(&id);
        free(title);
        return 0;
    }

    *id_out    = id.data;
    *title_out = title;
    return 1;
}

static int is_upper_title(const char *title)
{
    int has = 0;
    for (const char *p = title; *p; ++p) {
        if (isalpha((unsigned char)*p)) {
            has = 1;
            if (!isupper((unsigned char)*p))
                return 0;
        }
    }
    return has;
}

static char *normalize_alnum_upper_dup(const char *s)
{
    Str out;
    str_init(&out);

    for (const char *p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c))
            str_append_char(&out, (char)toupper(c));
    }

    return out.data;
}

typedef struct {
    char   *id;
    char   *title;
    char   *full;
    char   *title_norm;
    int     orig_line;
    int     page;
    double  y;
} Section;

typedef struct {
    int     orig_line;
    int     target;
    int     page;
    double  y;
    double  width;
    char   *text;
} Candidate;

typedef struct {
    int     page;
    int     orig_line;
    double  y;
    char   *text;
} VisLine;

static int find_section_by_id(Section *secs, size_t n, const char *id)
{
    if (!id || !id[0])
        return -1;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(secs[i].id, id) == 0)
            return (int)i;
    }
    return -1;
}

static int find_section_by_title_norm(Section *secs, size_t n, const char *norm)
{
    if (!norm || !norm[0])
        return -1;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(secs[i].title_norm, norm) == 0)
            return (int)i;
    }
    return -1;
}

static int add_section(Section **secs, size_t *n, size_t *cap,
                       const char *id, const char *title,
                       const char *full, int orig_line)
{
    if (*n == *cap) {
        *cap  = *cap ? *cap * 2 : 32;
        *secs = xrealloc(*secs, *cap * sizeof(Section));
    }

    Section *s = &(*secs)[*n];
    memset(s, 0, sizeof(*s));

    s->id         = xstrdup(id    ? id    : "");
    s->title      = xstrdup(title ? title : "");
    s->full       = xstrdup(full  ? full  : "");
    s->title_norm = normalize_alnum_upper_dup(s->title);
    s->orig_line  = orig_line;
    s->page       = 0;
    s->y          = 0.0;

    return (int)(*n)++;
}

static int add_candidate(Candidate **cands, size_t *n, size_t *cap,
                         int orig_line, int target)
{
    if (target < 0)
        return -1;

    if (*n == *cap) {
        *cap  = *cap ? *cap * 2 : 32;
        *cands = xrealloc(*cands, *cap * sizeof(Candidate));
    }

    Candidate *c = &(*cands)[*n];
    c->orig_line = orig_line;
    c->target    = target;
    c->page      = 0;
    c->y         = 0.0;
    c->width     = 0.0;
    c->text      = NULL;

    return (int)(*n)++;
}

/* ========================================================================
 * 3. Integration in 'main()'
 * ======================================================================== */


/* ================================================================
 * txt2pdf v1.7 patch additions
 *
 * - Base14 font selection: Courier / Helvetica / Times-Roman
 * - proportional wrapping using simple built-in metrics
 * - standalone [IMG]...[/IMG] images
 * ================================================================ */

typedef enum {
    FONT_COURIER = 0,
    FONT_HELVETICA,
    FONT_TIMES
} FontKind;

typedef enum {
    IMG_UNKNOWN = 0,
    IMG_JPEG,
    IMG_PNG_PASSTHROUGH
} ImageKind;

typedef struct {
    char          *path;
    unsigned char *data;
    size_t         data_len;

    ImageKind      kind;

    int            width;
    int            height;

    int            components; /* JPEG: 1 gray, 3 rgb, 4 cmyk */
    int            colors;     /* PNG predictor colors: 1 or 3 */
    int            indexed;    /* PNG palette */
    unsigned char *palette;
    size_t         palette_len;

    double         disp_w;
    double         disp_h;

    int            obj;
} Image;

typedef struct {
    int    page;
    int    image_idx;
    double x;
    double y;
    double w;
    double h;
} ImagePlace;

/* ------------------------------------------------------------
 * Font metrics: printable ASCII 32..126
 * ------------------------------------------------------------ */

static const unsigned short v17_helvetica_ascii[95] = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333,
    389, 584, 278, 333, 278, 278, 556, 556, 556, 556,
    556, 556, 556, 556, 556, 556, 278, 278, 584, 584,
    584, 556, 1015, 667, 667, 722, 722, 667, 611, 778,
    722, 278, 500, 667, 556, 833, 722, 778, 667, 778,
    722, 667, 611, 722, 667, 944, 667, 667, 611, 278,
    278, 278, 469, 556, 333, 556, 556, 500, 556, 556,
    278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500,
    500, 334, 260, 334, 584
};

static const unsigned short v17_times_ascii[95] = {
    250, 333, 408, 500, 500, 833, 778, 333, 333, 333,
    500, 564, 250, 333, 250, 278, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 278, 278, 564, 564,
    564, 444, 921, 722, 667, 667, 722, 611, 556, 722,
    722, 333, 389, 722, 611, 889, 722, 722, 556, 722,
    667, 556, 611, 722, 722, 944, 722, 722, 611, 333,
    278, 278, 469, 500, 333, 444, 500, 444, 500, 444,
    333, 500, 500, 278, 278, 500, 278, 778, 500, 500,
    500, 500, 333, 389, 278, 500, 500, 722, 500, 500,
    444, 480, 200, 480, 541
};

static int v17_is_space_char(unsigned char c)
{
    return c == ' ' || c == '\t' || c == 0xA0;
}

static unsigned char v17_width_base_char(unsigned char c)
{
    if (c == 0xA0)
        return ' ';

    if (c == 0xA1)
        return '!';
    if (c == 0xBF)
        return '?';

    if (c >= 0xC0 && c <= 0xC5)
        return 'A';
    if (c == 0xC6)
        return 'A';
    if (c == 0xC7)
        return 'C';
    if (c >= 0xC8 && c <= 0xCB)
        return 'E';
    if (c >= 0xCC && c <= 0xCF)
        return 'I';
    if (c == 0xD0)
        return 'D';
    if (c == 0xD1)
        return 'N';
    if (c >= 0xD2 && c <= 0xD6)
        return 'O';
    if (c == 0xD8)
        return 'O';
    if (c >= 0xD9 && c <= 0xDC)
        return 'U';
    if (c == 0xDD)
        return 'Y';

    if (c >= 0xE0 && c <= 0xE5)
        return 'a';
    if (c == 0xE6)
        return 'a';
    if (c == 0xE7)
        return 'c';
    if (c >= 0xE8 && c <= 0xEB)
        return 'e';
    if (c >= 0xEC && c <= 0xEF)
        return 'i';
    if (c == 0xF0)
        return 'd';
    if (c == 0xF1)
        return 'n';
    if (c >= 0xF2 && c <= 0xF6)
        return 'o';
    if (c == 0xF8)
        return 'o';
    if (c >= 0xF9 && c <= 0xFC)
        return 'u';
    if (c == 0xFD || c == 0xFF)
        return 'y';

    return c;
}

static unsigned short v17_glyph_width_1000(FontKind font, unsigned char c)
{
    if (font == FONT_COURIER)
        return 600;

    unsigned char b = v17_width_base_char(c);

    if (b >= 32 && b <= 126) {
        if (font == FONT_HELVETICA)
            return v17_helvetica_ascii[b - 32];
        else
            return v17_times_ascii[b - 32];
    }

    return 500;
}

static unsigned char v17_winansi_from_cp(int cp)
{
    if (cp >= 0x20 && cp <= 0x7E)
        return (unsigned char)cp;

    if (cp >= 0xA0 && cp <= 0xFF)
        return (unsigned char)cp;

    return '?';
}

static int v17_utf8_seq_len(unsigned char c)
{
    if (c < 0x80)
        return 1;

    if (c >= 0xC2 && c <= 0xDF)
        return 2;

    if (c >= 0xE0 && c <= 0xEF)
        return 3;

    if (c >= 0xF0 && c <= 0xF4)
        return 4;

    return 1;
}

static int v17_decode_utf8(const unsigned char *p,
                           size_t off,
                           size_t len,
                           int *cp,
                           size_t *consume)
{
    if (off >= len)
        return 0;

    unsigned char c = p[off];
    int need = v17_utf8_seq_len(c);

    if (need == 1) {
        *cp = c;
        *consume = 1;
        return 1;
    }

    if (off + (size_t)need > len)
        return 0;

    for (int k = 1; k < need; k++) {
        if ((p[off + (size_t)k] & 0xC0) != 0x80)
            return 0;
    }

    int v = 0;

    if (need == 2) {
        v = ((c & 0x1F) << 6) |
            (p[off + 1] & 0x3F);
    } else if (need == 3) {
        v = ((c & 0x0F) << 12) |
            ((p[off + 1] & 0x3F) << 6) |
            (p[off + 2] & 0x3F);
    } else {
        v = ((c & 0x07) << 18) |
            ((p[off + 1] & 0x3F) << 12) |
            ((p[off + 2] & 0x3F) << 6) |
            (p[off + 3] & 0x3F);
    }

    *cp = v;
    *consume = (size_t)need;
    return 1;
}

static void v17_advance_char(const char *s,
                             size_t off,
                             size_t slen,
                             unsigned char *out_c,
                             size_t *out_consume)
{
    const unsigned char *p = (const unsigned char *)s;

    if (g_is_utf8 && p[off] >= 0x80) {
        int cp = 0;
        size_t cons = 1;

        if (v17_decode_utf8(p, off, slen, &cp, &cons)) {
            *out_c = v17_winansi_from_cp(cp);
            *out_consume = cons;
            return;
        }
    }

    unsigned char c = p[off];

    if (c == '\t')
        c = ' ';

    *out_c = c;
    *out_consume = 1;
}

static double v17_text_width_pt(const char *s,
                                FontKind font,
                                double font_size,
                                int trim_trailing)
{
    size_t len = strlen(s);
    size_t off = 0;

    double total = 0.0;
    double non_space_total = 0.0;

    while (off < len) {
        unsigned char c = 0;
        size_t cons = 1;

        v17_advance_char(s, off, len, &c, &cons);

        double cw = font_size *
                    (double)v17_glyph_width_1000(font, c) /
                    1000.0;

        total += cw;

        if (!v17_is_space_char(c))
            non_space_total = total;

        off += cons;
    }

    return trim_trailing ? non_space_total : total;
}

static void v17_next_visual_chunk(const char *s,
                                  size_t off,
                                  size_t slen,
                                  double max_width,
                                  FontKind font,
                                  char **out_text,
                                  size_t *out_consume)
{
    double w = 0.0;
    size_t i = off;

    size_t last_space = 0;
    size_t last_punct = 0;

    while (i < slen) {
        unsigned char c = 0;
        size_t cons = 1;

        v17_advance_char(s, i, slen, &c, &cons);

        double cw = FONT_SIZE *
                    (double)v17_glyph_width_1000(font, c) /
                    1000.0;

        if (w + cw > max_width) {
            if (w == 0.0) {
                w += cw;
                i += cons;
            }
            break;
        }

        w += cw;
        i += cons;

        if (v17_is_space_char(c))
            last_space = i;
        else if (c == '-' || c == '/')
            last_punct = i;
    }

    if (i >= slen) {
        *out_text = xstrndup(s + off, slen - off);
        *out_consume = slen - off;
        return;
    }

    size_t break_i = 0;

    if (last_space > off)
        break_i = last_space;
    else if (last_punct > off)
        break_i = last_punct;
    else
        break_i = i;

    if (break_i <= off) {
        unsigned char c = 0;
        size_t cons = 1;

        v17_advance_char(s, off, slen, &c, &cons);
        break_i = off + cons;
    }

    if (break_i > slen)
        break_i = slen;

    *out_text = xstrndup(s + off, break_i - off);
    *out_consume = break_i - off;
}

static void v17_append_escaped_char(Str *out, unsigned char c)
{
    if (c == '\\')
        str_append_str(out, "\\\\");
    else if (c == '(')
        str_append_str(out, "\\(");
    else if (c == ')')
        str_append_str(out, "\\)");
    else if (c == '\r')
        str_append_str(out, "\\r");
    else if (c == '\n')
        str_append_str(out, "\\n");
    else if (c == '\t')
        str_append_char(out, ' ');
    else if (c >= 32)
        str_append_char(out, (char)c);
    else
        str_append_char(out, '?');
}

static void v17_pdf_escape(const char *s, Str *out)
{
    if (!out->data) {
        out->len = 0;
        out->cap = 0;
        str_init(out);
    } else {
        out->len = 0;
        if (out->cap > 0)
            out->data[0] = '\0';
    }

    const unsigned char *p = (const unsigned char *)s;
    size_t len = strlen(s);
    size_t i = 0;

    while (i < len) {
        if (g_is_utf8 && p[i] >= 0x80) {
            int cp = 0;
            size_t cons = 1;

            if (v17_decode_utf8(p, i, len, &cp, &cons)) {
                unsigned char w = v17_winansi_from_cp(cp);
                v17_append_escaped_char(out, w);
                i += cons;
                continue;
            }
        }

        v17_append_escaped_char(out, p[i]);
        i++;
    }
}

/* ------------------------------------------------------------
 * Image helpers
 * ------------------------------------------------------------ */

static char v17_ascii_tolower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (char)(c - 'A' + 'a');
    return c;
}

static int v17_str_ncasecmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char ca = v17_ascii_tolower(a[i]);
        char cb = v17_ascii_tolower(b[i]);

        if (ca != cb)
            return 0;

        if (ca == '\0')
            return 1;
    }

    return 1;
}

static int v17_has_extension(const char *s, const char *ext)
{
    size_t ls = strlen(s);
    size_t le = strlen(ext);

    if (ls < le)
        return 0;

    return v17_str_ncasecmp(s + ls - le, ext, le);
}

static int v17_has_jpeg_extension(const char *s)
{
    return v17_has_extension(s, ".jpg") ||
           v17_has_extension(s, ".jpeg");
}

static int v17_has_png_extension(const char *s)
{
    return v17_has_extension(s, ".png");
}

static unsigned v17_jpg_be16(const unsigned char *p)
{
    return (unsigned)((p[0] << 8) | p[1]);
}

static int v17_jpg_get_info(const unsigned char *p, size_t n,
                            int *w, int *h, int *comps)
{
    if (n < 2 || p[0] != 0xFF || p[1] != 0xD8)
        return 0;

    size_t i = 2;

    while (i + 4 <= n) {
        if (p[i] != 0xFF) {
            i++;
            continue;
        }

        while (i < n && p[i] == 0xFF)
            i++;

        if (i >= n)
            break;

        unsigned char marker = p[i++];

        if (marker == 0xD8 || marker == 0x01 ||
            (marker >= 0xD0 && marker <= 0xD9)) {
            continue;
        }

        if (marker == 0xDA)
            break;

        if (i + 2 > n)
            break;

        unsigned seglen = (unsigned)((p[i] << 8) | p[i + 1]);
        if (seglen < 2 || i + seglen > n)
            break;

        if (marker >= 0xC0 && marker <= 0xCF &&
            marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
            if (i + 8 <= n) {
                *h = (int)v17_jpg_be16(p + i + 3);
                *w = (int)v17_jpg_be16(p + i + 5);
                *comps = (int)p[i + 7];
                return 1;
            }
        }

        i += seglen;
    }

    return 0;
}

static unsigned v17_png_be32(const unsigned char *p)
{
    return ((unsigned)p[0] << 24) |
           ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8)  |
           ((unsigned)p[3]);
}

static int v17_load_jpeg_passthrough(const char *path, Image *img)
{
    size_t len = 0;
    char *buf = read_file(path, &len);

    if (!buf)
        return 0;

    int w = 0, h = 0, comp = 0;

    if (!v17_jpg_get_info((const unsigned char *)buf, len, &w, &h, &comp)) {
        free(buf);
        return 0;
    }

    if (comp != 1 && comp != 3 && comp != 4)
        comp = 3;

    img->data = (unsigned char *)buf;
    img->data_len = len;
    img->kind = IMG_JPEG;
    img->width = w;
    img->height = h;
    img->components = comp;
    img->colors = 0;
    img->indexed = 0;
    img->palette = NULL;
    img->palette_len = 0;

    return 1;
}

static int v17_load_png_passthrough(const char *path, Image *img)
{
    static const unsigned char png_sig[8] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };

    size_t len = 0;
    char *buf = read_file(path, &len);

    if (!buf)
        return 0;

    const unsigned char *p = (const unsigned char *)buf;

    if (len < 33 || memcmp(p, png_sig, 8) != 0) {
        free(buf);
        return 0;
    }

    int width      = (int)v17_png_be32(p + 16);
    int height     = (int)v17_png_be32(p + 20);
    int bit_depth  = p[24];
    int color_type = p[25];
    int interlace  = p[28];

    if (width <= 0 || height <= 0) {
        free(buf);
        return 0;
    }

    /* MVP: 8-bit, non-interlaced, gray/RGB/palette only. */
    if (bit_depth != 8 || interlace != 0 ||
        !(color_type == 0 || color_type == 2 || color_type == 3)) {
        free(buf);
        return 0;
    }

    Str idat;
    str_init(&idat);

    unsigned char *palette = NULL;
    size_t palette_len = 0;

    size_t pos = 8;

    while (pos + 12 <= len) {
        unsigned clen = v17_png_be32(p + pos);
        const unsigned char *type  = p + pos + 4;
        const unsigned char *cdata = p + pos + 8;

        if (pos + 12 + clen > len)
            break;

        if (memcmp(type, "IDAT", 4) == 0) {
            str_append(&idat, (const char *)cdata, clen);
        } else if (memcmp(type, "PLTE", 4) == 0) {
            free(palette);
            palette = xmalloc(clen);
            memcpy(palette, cdata, clen);
            palette_len = clen;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }

        pos += 12 + clen;
    }

    free(buf);

    if (idat.len == 0 || (color_type == 3 && palette_len == 0)) {
        str_free(&idat);
        free(palette);
        return 0;
    }

    img->data = (unsigned char *)idat.data;
    img->data_len = idat.len;
    img->kind = IMG_PNG_PASSTHROUGH;
    img->width = width;
    img->height = height;
    img->components = 0;
    img->colors = (color_type == 2) ? 3 : 1;
    img->indexed = (color_type == 3);
    img->palette = palette;
    img->palette_len = palette_len;

    return 1;
}

static void v17_compute_image_display_size(Image *img, double max_h)
{
    double avail_w = PAGE_WIDTH - 2.0 * MARGIN;
    double avail_h = max_h;

    if (avail_h <= 0.0)
        avail_h = PAGE_HEIGHT - 2.0 * MARGIN;

    double w = (double)img->width;
    double h = (double)img->height;

    double scale = 1.0;

    if (w > avail_w)
        scale = avail_w / w;

    if (h * scale > avail_h)
        scale = avail_h / h;

    if (scale <= 0.0)
        scale = 1.0;

    img->disp_w = w * scale;
    img->disp_h = h * scale;

    if (img->disp_w < 1.0)
        img->disp_w = 1.0;

    if (img->disp_h < 1.0)
        img->disp_h = 1.0;
}

static int v17_parse_img_line(const char *line, char **path_out)
{
    const char *open = strstr(line, "[IMG]");

    if (!open || open != line)
        return 0;

    open += 5;

    const char *close = strstr(open, "[/IMG]");
    if (!close)
        return 0;

    const char *end = close;
    while (end > open && isspace((unsigned char)end[-1]))
        end--;

    const char *after = close + 6;
    while (isspace((unsigned char)*after))
        after++;

    if (*after != '\0')
        return 0;

    if (end <= open)
        return 0;

    *path_out = xstrndup(open, (size_t)(end - open));
    return 1;
}

static int v17_add_image(Image **imgs, size_t *n, size_t *cap, Image *img)
{
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *imgs = xrealloc(*imgs, *cap * sizeof(Image));
    }

    (*imgs)[*n] = *img;
    return (int)(*n)++;
}

static int v17_add_image_placement(ImagePlace **places, size_t *n, size_t *cap,
                                   int page, int image_idx,
                                   double x, double y, double w, double h)
{
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 32;
        *places = xrealloc(*places, *cap * sizeof(ImagePlace));
    }

    ImagePlace *p = &(*places)[*n];

    p->page = page;
    p->image_idx = image_idx;
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;

    return (int)(*n)++;
}

/* ================================================================
 * v1.7 main()
 * ================================================================ */

int main(int argc, char **argv)
{
    setlocale(LC_NUMERIC, "C");

    int debug = 0;
    FontKind font_kind = FONT_COURIER;
    const char *input = NULL;
    const char *output = NULL;

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("Usage: %s [--debug] [--sans|--serif|--mono] input.txt output.pdf\n",
               argv[0]);
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "--sans") == 0) {
            font_kind = FONT_HELVETICA;
        } else if (strcmp(argv[i], "--serif") == 0) {
            font_kind = FONT_TIMES;
        } else if (strcmp(argv[i], "--mono") == 0) {
            font_kind = FONT_COURIER;
        } else if (!input) {
            input = argv[i];
        } else if (!output) {
            output = argv[i];
        }
    }

    if (!input || !output) {
        fprintf(stderr,
                "Usage: %s [--debug] [--sans|--serif|--mono] input.txt output.pdf\n",
                argv[0]);
        return 1;
    }

    size_t len = 0;
    char *data = read_file(input, &len);

    if (!data) {
        fprintf(stderr, "Cannot read '%s'\n", input);
        return 1;
    }

    g_is_utf8 = detect_if_utf8(data, len);

    if (debug) {
        fprintf(stderr, "[debug] Detected encoding: %s\n",
                g_is_utf8 ? "UTF-8" : "Latin-1/ISO-8859-1");
        fprintf(stderr, "[debug] Font: %s\n",
                font_kind == FONT_HELVETICA ? "Helvetica" :
                font_kind == FONT_TIMES ? "Times-Roman" : "Courier");
    }

    int nlines = 0;
    char **lines = split_lines(data, len, &nlines);

    free(data);
    data = NULL;

    int map_n = nlines > 0 ? nlines : 1;

    int *heading_map   = xmalloc(sizeof(int) * (size_t)map_n);
    int *candidate_map = xmalloc(sizeof(int) * (size_t)map_n);
    int *toc_map       = xmalloc(sizeof(int) * (size_t)map_n);
    int *image_map     = xmalloc(sizeof(int) * (size_t)map_n);

    for (int i = 0; i < map_n; i++) {
        heading_map[i] = -1;
        candidate_map[i] = -1;
        toc_map[i] = 0;
        image_map[i] = -1;
    }

    Section   *sections  = NULL;
    size_t     nsections = 0, capsections = 0;
    Candidate *cands     = NULL;
    size_t     ncands    = 0, capcands = 0;
    VisLine   *vis       = NULL;
    size_t     nvis      = 0, capvis = 0;

    Image      *images    = NULL;
    size_t      nimages   = 0, capimages = 0;
    ImagePlace *places    = NULL;
    size_t      nplaces   = 0, capplaces = 0;

    int    *outline_idx = NULL;
    long   *offsets     = NULL;
    FILE   *out         = NULL;
    int     rc          = 0;
    int     toc_line_count = 0;

    Str esc;
    esc.data = NULL;
    esc.len = 0;
    esc.cap = 0;

    double max_width = PAGE_WIDTH - 2.0 * MARGIN;

    int max_chars = (int)(max_width / CHAR_WIDTH);
    if (max_chars < 1)
        max_chars = 1;

    int lines_per_page = (int)((PAGE_HEIGHT - 2.0 * MARGIN) / LEADING);
    if (lines_per_page < 1)
        lines_per_page = 1;

    double max_image_h = (double)lines_per_page * LEADING;

    /* ---- Detect TOC block ---- */
    for (int i = 0; i < nlines; i++) {
        if (toc_map[i])
            continue;

        char *trimmed = trim_dup(lines[i]);
        int is_toc_title = (strstr(trimmed, "[TOC]") != NULL);
        free(trimmed);

        if (!is_toc_title)
            continue;

        int j = i + 1;
        while (j < nlines && line_is_blank(lines[j]))
            j++;

        if (j < nlines && is_separator(lines[j])) {
            int k = j + 1;
            while (k < nlines && !is_separator(lines[k]))
                k++;

            int end = (k < nlines) ? k : (nlines - 1);

            for (int x = i; x <= end; x++) {
                toc_map[x] = 1;
                toc_line_count++;
            }

            i = end;
        }
    }

    /* ---- Detect sections ---- */
    for (int i = 0; i < nlines; i++) {
        char *id = NULL, *title = NULL;

        if (!parse_section_line_dyn(lines[i], &id, &title))
            continue;

        int valid = is_upper_title(title);

        if (valid && toc_map[i]) {
            if (debug)
                fprintf(stderr, "[debug] SKIP line %d: heading inside TOC\n", i + 1);
            valid = 0;
        }

        if (valid &&
            (!nearby_prev_is_separator(lines, nlines, i) ||
             !nearby_next_is_separator(lines, nlines, i))) {
            if (debug)
                fprintf(stderr,
                        "[debug] SKIP line %d: heading not between separators\n",
                        i + 1);
            valid = 0;
        }

        if (valid) {
            char *full = trim_dup(lines[i]);
            int idx = add_section(&sections, &nsections, &capsections,
                                  id, title, full, i);
            heading_map[i] = idx;
            free(full);
        }

        free(id);
        free(title);
    }

    /* ---- Detect TOC links ---- */
    for (int i = 0; i < nlines; i++) {
        if (!toc_map[i])
            continue;

        if (candidate_map[i] != -1)
            continue;

        char *id = NULL, *title = NULL;

        if (!parse_section_line_dyn(lines[i], &id, &title))
            continue;

        int target = find_section_by_id(sections, nsections, id);

        if (target < 0) {
            char *norm = normalize_alnum_upper_dup(title);
            target = find_section_by_title_norm(sections, nsections, norm);
            free(norm);
        }

        if (target >= 0) {
            int idx = add_candidate(&cands, &ncands, &capcands, i, target);
            if (idx >= 0)
                candidate_map[i] = idx;
        }

        free(id);
        free(title);
    }

    /* ---- Detect images ---- */
    for (int i = 0; i < nlines; i++) {
        char *trimmed = trim_dup(lines[i]);
        char *imgpath = NULL;

        if (v17_parse_img_line(trimmed, &imgpath)) {
            Image img;
            memset(&img, 0, sizeof(img));
            img.path = xstrdup(imgpath);

            int ok = 0;

            if (v17_has_jpeg_extension(imgpath))
                ok = v17_load_jpeg_passthrough(imgpath, &img);
            else if (v17_has_png_extension(imgpath))
                ok = v17_load_png_passthrough(imgpath, &img);

            if (ok) {
                v17_compute_image_display_size(&img, max_image_h);
                image_map[i] = v17_add_image(&images, &nimages, &capimages, &img);

                if (debug) {
                    fprintf(stderr,
                            "[debug] IMAGE line %d path '%s' %dx%d disp %.2fx%.2f\n",
                            i + 1, img.path, img.width, img.height,
                            img.disp_w, img.disp_h);
                }
            } else {
                if (debug)
                    fprintf(stderr, "[debug] Cannot load image: '%s'\n", imgpath);

                free(img.path);
            }

            free(imgpath);
        }

        free(trimmed);
    }

    /* ---- Page layout ---- */
    int current_page = 1;
    int line_on_page = 0;

    for (int i = 0; i < nlines; i++) {
        if (image_map[i] >= 0) {
            Image *img = &images[image_map[i]];

            int needed = (int)(img->disp_h / LEADING);
            if ((double)needed * LEADING < img->disp_h)
                needed++;

            if (needed < 1)
                needed = 1;

            if (needed > lines_per_page)
                needed = lines_per_page;

            if (line_on_page + needed > lines_per_page) {
                current_page++;
                line_on_page = 0;
            }

            double top = PAGE_HEIGHT - MARGIN - (double)line_on_page * LEADING;
            double y = top - img->disp_h;

            if (y < MARGIN)
                y = MARGIN;

            v17_add_image_placement(&places, &nplaces, &capplaces,
                                    current_page, image_map[i],
                                    MARGIN, y, img->disp_w, img->disp_h);

            line_on_page += needed;
            continue;
        }

        const char *s = lines[i];
        size_t slen = strlen(s);
        size_t off = 0;
        int first = 1, emitted = 0;

        while (off < slen || !emitted) {
            char *buf = NULL;
            size_t consume = 0;

            if (off >= slen) {
                buf = xstrdup("");
                consume = 0;
            } else if (font_kind == FONT_COURIER) {
                next_visual_chunk(s, off, slen, max_chars, &buf, &consume);
            } else {
                v17_next_visual_chunk(s, off, slen, max_width, font_kind,
                                      &buf, &consume);
            }

            if (consume == 0)
                consume = 1;

            if (line_on_page >= lines_per_page) {
                current_page++;
                line_on_page = 0;
            }

            double y = PAGE_HEIGHT - MARGIN - (double)line_on_page * LEADING;

            if (nvis == capvis) {
                capvis = capvis ? capvis * 2 : 256;
                vis = xrealloc(vis, capvis * sizeof(VisLine));
            }

            vis[nvis].page = current_page;
            vis[nvis].orig_line = i;
            vis[nvis].y = y;
            vis[nvis].text = buf;
            nvis++;

            if (first) {
                if (heading_map[i] >= 0) {
                    sections[heading_map[i]].page = current_page;
                    sections[heading_map[i]].y = y;
                }

                if (candidate_map[i] >= 0) {
                    Candidate *c = &cands[candidate_map[i]];
                    c->page = current_page;
                    c->y = y;
                    free(c->text);
                    c->text = xstrdup(buf);

                    if (font_kind == FONT_COURIER)
                        c->width = visible_width(buf);
                    else
                        c->width = v17_text_width_pt(buf, font_kind, FONT_SIZE, 1);
                }

                first = 0;
            }

            off += consume;
            emitted = 1;
            line_on_page++;
        }
    }

    int total_pages = current_page;
    if (total_pages < 1)
        total_pages = 1;

    /* ---- Outline ---- */
    size_t outline_count = 0;

    if (nsections > 0) {
        outline_idx = xmalloc(sizeof(int) * nsections);

        for (size_t i = 0; i < nsections; i++) {
            if (sections[i].page > 0)
                outline_idx[outline_count++] = (int)i;
        }
    }

    int K = (int)outline_count;

    if (debug) {
        fprintf(stderr, "[debug] Lines marked as TOC: %d\n", toc_line_count);
        fprintf(stderr, "[debug] Sections detected: %zu\n", nsections);
        fprintf(stderr, "[debug] Images detected: %zu\n", nimages);
        fprintf(stderr, "[debug] Image placements: %zu\n", nplaces);
    }

    const char *basefont = "Courier";

    if (font_kind == FONT_HELVETICA)
        basefont = "Helvetica";
    else if (font_kind == FONT_TIMES)
        basefont = "Times-Roman";

    /* ---- Generate PDF ---- */
    out = fopen(output, "wb");

    if (!out) {
        fprintf(stderr, "Cannot write '%s'\n", output);
        rc = 1;
        goto cleanup;
    }

    int first_page_obj    = 5;
    int first_content_obj = first_page_obj + total_pages;
    int first_outline_obj = first_content_obj + total_pages;
    int first_image_obj   = first_outline_obj + K;
    int total_objs        = 4 + 2 * total_pages + K + (int)nimages;

    for (size_t i = 0; i < nimages; i++)
        images[i].obj = first_image_obj + (int)i;

    offsets = xmalloc(sizeof(long) * (size_t)(total_objs + 1));

    fprintf(out, "%%PDF-1.4\n");

    static const unsigned char bincomment[] = {
        '%', 0xE2, 0xE3, 0xCF, 0xD3, '\n'
    };

    fwrite(bincomment, 1, sizeof(bincomment), out);

    /* Catalog */
    offsets[1] = ftell(out);

    if (K > 0) {
        fprintf(out,
                "1 0 obj\n"
                "<< /Type /Catalog /Pages 3 0 R /Outlines 2 0 R /PageMode /UseOutlines >>\n"
                "endobj\n");
    } else {
        fprintf(out,
                "1 0 obj\n"
                "<< /Type /Catalog /Pages 3 0 R >>\n"
                "endobj\n");
    }

    /* Outlines root */
    offsets[2] = ftell(out);

    if (K > 0) {
        fprintf(out,
                "2 0 obj\n"
                "<< /Type /Outlines /First %d 0 R /Last %d 0 R /Count %d >>\n"
                "endobj\n",
                first_outline_obj,
                first_outline_obj + K - 1,
                K);
    } else {
        fprintf(out,
                "2 0 obj\n"
                "<< /Type /Outlines /Count 0 >>\n"
                "endobj\n");
    }

    /* Pages */
    offsets[3] = ftell(out);

    Str kids;
    str_init(&kids);
    str_append_str(&kids, "[");

    for (int p = 1; p <= total_pages; p++) {
        if (p > 1)
            str_append_char(&kids, ' ');

        str_appendf(&kids, "%d 0 R", first_page_obj + p - 1);
    }

    str_append_str(&kids, "]");

    fprintf(out,
            "3 0 obj\n"
            "<< /Type /Pages /Kids %s /Count %d >>\n"
            "endobj\n",
            kids.data, total_pages);

    str_free(&kids);

    /* Font */
    offsets[4] = ftell(out);

    fprintf(out,
            "4 0 obj\n"
            "<< /Type /Font /Subtype /Type1 /BaseFont /%s /Encoding /WinAnsiEncoding >>\n"
            "endobj\n",
            basefont);

    str_init(&esc);

    /* Pages */
    for (int p = 1; p <= total_pages; p++) {
        int page_obj    = first_page_obj + p - 1;
        int content_obj = first_content_obj + p - 1;

        Str annots;
        str_init(&annots);
        str_append_str(&annots, "[");

        int has_annots = 0;

        for (size_t ci = 0; ci < ncands; ci++) {
            Candidate *c = &cands[ci];

            if (c->page == p &&
                c->target >= 0 &&
                c->text &&
                c->width > 1.0) {
                Section *s = &sections[c->target];

                if (s->page > 0) {
                    int target_page_obj = first_page_obj + s->page - 1;
                    double dest_y = s->y + 10.0;

                    if (dest_y > PAGE_HEIGHT - MARGIN)
                        dest_y = PAGE_HEIGHT - MARGIN;

                    double x1 = MARGIN - 1.0;
                    double y1 = c->y - 2.0;
                    double x2 = MARGIN + c->width + 1.0;
                    double y2 = c->y + FONT_SIZE;

                    if (has_annots)
                        str_append_char(&annots, ' ');

                    str_appendf(&annots,
                                "<< /Type /Annot /Subtype /Link "
                                "/Rect [%.2f %.2f %.2f %.2f] "
                                "/Border [0 0 0] "
                                "/Dest [%d 0 R /XYZ %.2f %.2f null] >>",
                                x1, y1, x2, y2,
                                target_page_obj,
                                MARGIN, dest_y);

                    has_annots = 1;
                }
            }
        }

        str_append_str(&annots, "]");

        Str xobjs;
        str_init(&xobjs);

        int has_xobjs = 0;

        for (size_t pi = 0; pi < nplaces; pi++) {
            ImagePlace *pl = &places[pi];

            if (pl->page != p)
                continue;

            if (has_xobjs)
                str_append_char(&xobjs, ' ');

            str_appendf(&xobjs, "/Im%d %d 0 R",
                        pl->image_idx + 1,
                        images[pl->image_idx].obj);

            has_xobjs = 1;
        }

        offsets[page_obj] = ftell(out);

        fprintf(out,
                "%d 0 obj\n"
                "<< /Type /Page /Parent 3 0 R "
                "/MediaBox [0 0 %.0f %.0f] "
                "/Resources << /Font << /F1 4 0 R >>",
                page_obj, PAGE_WIDTH, PAGE_HEIGHT);

        if (has_xobjs)
            fprintf(out, " /XObject << %s >>", xobjs.data);

        fprintf(out, " >> /Contents %d 0 R", content_obj);

        if (has_annots)
            fprintf(out, " /Annots %s", annots.data);

        fprintf(out, " >>\nendobj\n");

        str_free(&annots);
        str_free(&xobjs);
    }

    /* Content streams */
    for (int p = 1; p <= total_pages; p++) {
        int content_obj = first_content_obj + p - 1;

        Str content;
        str_init(&content);

        for (size_t pi = 0; pi < nplaces; pi++) {
            ImagePlace *pl = &places[pi];

            if (pl->page != p)
                continue;

            str_appendf(&content,
                        "q\n"
                        "%.2f 0 0 %.2f %.2f %.2f cm\n"
                        "/Im%d Do\n"
                        "Q\n",
                        pl->w, pl->h, pl->x, pl->y,
                        pl->image_idx + 1);
        }

        for (size_t vi = 0; vi < nvis; vi++) {
            if (vis[vi].page == p &&
                vis[vi].text &&
                vis[vi].text[0]) {
                v17_pdf_escape(vis[vi].text, &esc);

                str_appendf(&content,
                            "BT /F1 %.1f Tf %.2f %.2f Td (%s) Tj ET\n",
                            FONT_SIZE, MARGIN, vis[vi].y,
                            esc.data ? esc.data : "");
            }
        }

        offsets[content_obj] = ftell(out);

        fprintf(out,
                "%d 0 obj\n"
                "<< /Length %zu >>\n"
                "stream\n",
                content_obj, content.len);

        if (content.len > 0)
            fwrite(content.data, 1, content.len, out);

        fprintf(out, "endstream\nendobj\n");

        str_free(&content);
    }

    /* Outline items */
    for (int j = 0; j < K; j++) {
        int      si       = outline_idx[j];
        Section *s        = &sections[si];
        int      obj      = first_outline_obj + j;
        int      page_obj = first_page_obj + s->page - 1;
        double   dest_y   = s->y + 10.0;

        if (dest_y > PAGE_HEIGHT - MARGIN)
            dest_y = PAGE_HEIGHT - MARGIN;

        v17_pdf_escape(s->full, &esc);

        offsets[obj] = ftell(out);

        fprintf(out,
                "%d 0 obj\n"
                "<< /Title (%s) /Parent 2 0 R",
                obj, esc.data ? esc.data : "");

        if (j > 0)
            fprintf(out, " /Prev %d 0 R", obj - 1);

        if (j < K - 1)
            fprintf(out, " /Next %d 0 R", obj + 1);

        fprintf(out,
                " /Dest [%d 0 R /XYZ %.2f %.2f null] >>\n"
                "endobj\n",
                page_obj, MARGIN, dest_y);
    }

    /* Image XObjects */
    for (size_t ii = 0; ii < nimages; ii++) {
        Image *img = &images[ii];

        offsets[img->obj] = ftell(out);

        if (img->kind == IMG_JPEG) {
            const char *cs = "/DeviceRGB";

            if (img->components == 1)
                cs = "/DeviceGray";
            else if (img->components == 4)
                cs = "/DeviceCMYK";

            fprintf(out,
                    "%d 0 obj\n"
                    "<< /Type /XObject /Subtype /Image\n"
                    "   /Width %d /Height %d\n"
                    "   /ColorSpace %s\n"
                    "   /BitsPerComponent 8\n"
                    "   /Filter /DCTDecode\n"
                    "   /Length %zu >>\n"
                    "stream\n",
                    img->obj,
                    img->width,
                    img->height,
                    cs,
                    img->data_len);

            fwrite(img->data, 1, img->data_len, out);
            fwrite("\nendstream\nendobj\n", 1,
                   sizeof("\nendstream\nendobj\n") - 1, out);
        } else if (img->kind == IMG_PNG_PASSTHROUGH) {
            Str cs;
            str_init(&cs);

            if (img->indexed && img->palette_len >= 3) {
                str_appendf(&cs, "[/Indexed /DeviceRGB %zu <",
                            img->palette_len / 3 - 1);

                for (size_t k = 0; k < img->palette_len; k++)
                    str_appendf(&cs, "%02X", img->palette[k]);

                str_append_str(&cs, ">]");
            } else if (img->colors == 1) {
                str_append_str(&cs, "/DeviceGray");
            } else {
                str_append_str(&cs, "/DeviceRGB");
            }

            fprintf(out,
                    "%d 0 obj\n"
                    "<< /Type /XObject /Subtype /Image\n"
                    "   /Width %d /Height %d\n"
                    "   /ColorSpace %s\n"
                    "   /BitsPerComponent 8\n"
                    "   /Filter /FlateDecode\n"
                    "   /DecodeParms << /Predictor 15 /Colors %d /BitsPerComponent 8 /Columns %d >>\n"
                    "   /Length %zu >>\n"
                    "stream\n",
                    img->obj,
                    img->width,
                    img->height,
                    cs.data,
                    img->colors,
                    img->width,
                    img->data_len);

            fwrite(img->data, 1, img->data_len, out);
            fwrite("\nendstream\nendobj\n", 1,
                   sizeof("\nendstream\nendobj\n") - 1, out);

            str_free(&cs);
        }
    }

    /* Xref + trailer */
    long xref_pos = ftell(out);

    fprintf(out, "xref\n0 %d\n", total_objs + 1);
    fprintf(out, "0000000000 65535 f \n");

    for (int i = 1; i <= total_objs; i++)
        fprintf(out, "%010ld 00000 n \n", offsets[i]);

    fprintf(out,
            "trailer\n"
            "<< /Size %d /Root 1 0 R >>\n"
            "startxref\n"
            "%ld\n",
            total_objs + 1, xref_pos);

    fwrite("%%EOF\n", 1, sizeof("%%EOF\n") - 1, out);

    printf("PDF generated: %s\n", output);
    printf("Pages: %d | Bookmarks: %d | Internal links: %zu | Images: %zu | Font: %s\n",
           total_pages, K, ncands, nimages, basefont);

    rc = 0;

cleanup:
    if (out)
        fclose(out);

    if (lines) {
        for (int i = 0; i < nlines; i++)
            free(lines[i]);
        free(lines);
    }

    free(heading_map);
    free(candidate_map);
    free(toc_map);
    free(image_map);

    if (sections) {
        for (size_t i = 0; i < nsections; i++) {
            free(sections[i].id);
            free(sections[i].title);
            free(sections[i].full);
            free(sections[i].title_norm);
        }
        free(sections);
    }

    if (cands) {
        for (size_t i = 0; i < ncands; i++)
            free(cands[i].text);
        free(cands);
    }

    if (vis) {
        for (size_t i = 0; i < nvis; i++)
            free(vis[i].text);
        free(vis);
    }

    if (images) {
        for (size_t i = 0; i < nimages; i++) {
            free(images[i].path);
            free(images[i].data);
            free(images[i].palette);
        }
        free(images);
    }

    free(places);
    free(outline_idx);
    free(offsets);
    str_free(&esc);

    return rc;
}
