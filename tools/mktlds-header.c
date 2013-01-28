/*
 * Copyright (c) 2013 Elias Norberg <xyzzy@kudzu.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <ctype.h>

#define P_BASE		(36)
#define P_TMIN		(1)
#define P_TMAX		(26)
#define P_SKEW		(38)
#define P_DAMP		(700)
#define INITIAL_BIAS	(72)
#define INITIAL_N	(128)

int
adapt(int delta, int numpoints, int firsttime)
{
    int		k;

    if (firsttime)
        delta = delta / P_DAMP;
    else
        delta = delta / 2;

    delta += (delta / numpoints);

    k = 0;
    while (delta > (((P_BASE - P_TMIN) * P_TMAX) / 2)) {
        delta = delta / (P_BASE - P_TMIN);
        k += P_BASE;
    }

    k += (((P_BASE - P_TMIN + 1) * delta) / (delta + P_SKEW));
    return (k);
}

int
get_minimum_char(char *str, gunichar n)
{
    gunichar	ch = 0;
    gunichar	min = 0xffffff;

    for(; *str; str = g_utf8_next_char(str)) {
        ch = g_utf8_get_char(str);
        if (ch >= n && ch < min)
            min = ch;
    }

    return (min);
}

char
encode_digit(int n)
{
    if (n < 26)
        return n + 'a';
    return (n - 26) + '0';
}

char *
punycode_encode_part(char *str)
{
    char		output[1024];
    char		*s;
    gunichar	c;
    int		need_coding = 0;
    int		l, len, i;

    gunichar	n = INITIAL_N;
    int		delta = 0;
    int		bias = INITIAL_BIAS;
    int		h, b, m, k, t, q;

    l = 0;
    for (s=str; *s; s = g_utf8_next_char(s)) {
        c = g_utf8_get_char(s);
        if (c < 128)
            output[l++] = *s;
        else
            need_coding = 1;
    }

    output[l] = '\0';

    if (!need_coding)
        return g_strdup(output);

    h = b = strlen(output);

    if (l > 0)
        output[l++] = '-';
    output[l] = '\0';

    len = g_utf8_strlen(str, -1);
    while (h < len) {
        m = get_minimum_char(str, n);
        delta += (m - n) * (h + 1);
        n = m;
        for (s=str; *s; s = g_utf8_next_char(s)) {
            c = g_utf8_get_char(s);

            if (c < n) delta ++;
            if (c == n) {
                q = delta;
                for (k=P_BASE;; k+=P_BASE) {
                    if (k <= bias)
                        t = P_TMIN;
                    else if(k >= bias + P_TMAX)
                        t = P_TMAX;
                    else
                        t = k - bias;

                    if (q < t)
                        break;

                    output[l++] = encode_digit(t+((q-t)%(P_BASE-t)));
                    q = (q - t) / (P_BASE - t);
                }
                output[l++] = encode_digit(q);
                bias = adapt(delta, h + 1, h == b);
                delta = 0;
                h ++;
            }
        }
        delta ++;
        n ++;
    }

    output[l] = '\0';
    for (i=l+4;i>=4;i--)
        output[i] = output[i-4];
    l += 4;
    output[0] = 'x';
    output[1] = 'n';
    output[2] = '-';
    output[3] = '-';
    output[l] = '\0';
    return g_strdup(output);
}

char *
punycode_encode(const char *host)
{
    char enc_str[512];
    char *enc_lbl, *ptr;
    char *next_lbl;
    char *host_dup;

    enc_str[0] = '\0';
    host_dup = strdup(host);
    ptr = host_dup;
    for (;;) {
        if ((next_lbl = strchr(ptr, '.')))
            *next_lbl = '\0';

        enc_lbl = punycode_encode_part(ptr);

        strcat(enc_str, enc_lbl);
        if (next_lbl)
            strcat(enc_str, ".");
        g_free(enc_lbl);

        if (!next_lbl)
            break;
        ptr = next_lbl + 1;
    }

    g_free(host_dup);
    return g_strdup(enc_str);
}

int main()
{
    char buf[512];
    char *ptr;

    printf("#ifndef TLDS_H\n");
    printf("#define TLDS_H\n");
    printf("static char *TLDS_EFFECTIVE[] = {\n");

    while (!feof(stdin)) {
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;

        for (ptr = buf+strlen(buf)-1; isspace(*ptr) ||
            *ptr == '\n' || *ptr == '\r'; ptr --)
            *ptr = '\0';

        if (buf[0] == '\0') continue;

        if (buf[0] == '/' && buf[1] == '/')
            printf("%s\n", buf);
        else {
            char *encoded = punycode_encode(buf);
            printf("\"%s\",\n", encoded);
            g_free(encoded);
        }
    }
    printf("NULL,\n");
    printf("};\n");
    printf("#endif\n");

    return 0;
}
