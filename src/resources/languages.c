#include "resources/languages.h"
#include "resources/fonts.h"

#include "formats/error.h"
#include "formats/language.h"
#include "game/utils/settings.h"
#include "resource_files.h"
#include "utils/allocator.h"
#include "utils/c_array_util.h"
#include "utils/c_string_util.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static sd_language *language = NULL;
static sd_language *language2 = NULL;

// Fall back to the default language if the configured one does not resolve.
static void ensure_valid_language_setting(settings_language *lang_settings) {
    const path configured = get_resource_filename(lang_settings->language);
    if(!path_exists(&configured)) {
        log_error("Configured language file '%s' not found, falling back to 'ENGLISH.DAT'.", path_c(&configured));
        omf_free(lang_settings->language);
        lang_settings->language = omf_strdup("ENGLISH.DAT");
    }
}

bool lang_init(void) {
    language = NULL;
    language2 = NULL;

    settings_language *lang_settings = &settings_get()->language;
    ensure_valid_language_setting(lang_settings);
    const path language_file1 = get_resource_filename(lang_settings->language);

    // Load up language file
    language = omf_calloc(1, sizeof(sd_language));
    sd_language_create(language);
    // Note: we don't load descriptions -- we don't use them.
    if(sd_language_load(language, &language_file1, false)) {
        log_error("Unable to load language file '%s'!", path_c(&language_file1));
        goto error_0;
    }

    // Trim off the last linebreak from the translation texts. We don't need it.
    for(unsigned int i = 0; i < vector_size(&language->strings); i++) {
        sd_lang_string *entry = vector_get(&language->strings, i);
        if(str_ends_with(&entry->data, "\n")) {
            str_cut(&entry->data, 1);
        }
    }

    // OMF GERMAN.DAT and old versions of ENGLISH.DAT have only 990 strings
    unsigned int const old_language_count = 990;

    if(vector_size(&language->strings) == old_language_count) {
        // OMF 2.1 added netplay, and with it 23 new localization strings. Insert empty
        // placeholders at their ids (ascending) so the original strings shift up to their
        // 1013-entry positions.
        const unsigned new_ids[] = {149, 150, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181,
                                    182, 183, 184, 185, 267, 269, 270, 271, 284, 295, 305};
        for(unsigned i = 0; i < N_ELEMENTS(new_ids); i++) {
            sd_lang_string dummy;
            str_from_c(&dummy.description, "dummy");
            str_create(&dummy.data);
            vector_insert_at(&language->strings, new_ids[i], &dummy);
        }
    }
    if(vector_size(&language->strings) != LANG_STR_COUNT) {
        log_error("Unable to load language file '%s', unsupported or corrupt file!", path_c(&language_file1));
        goto error_0;
    }

    log_info("Loaded language file '%s'.", path_c(&language_file1));

    // Korean translation: strings are UTF-8 (not CP437) and Hangul is drawn from KOREAN.FNT
    fonts_set_utf8(strncasecmp(lang_settings->language, "KOREAN", 6) == 0);

    // Load up language2 file (OpenOMF)
    path language_file2 = get_resource_filename(lang_settings->language);
    str ext;
    path_ext(&language_file2, &ext);
    str_append_char(&ext, '2');
    path_set_ext(&language_file2, str_c(&ext));
    str_free(&ext);

    language2 = omf_calloc(1, sizeof(sd_language));
    sd_language_create(language2);
    // Note: we don't load descriptions -- we don't use them.
    if(sd_language_load(language2, &language_file2, false)) {
        log_error("Unable to load OpenOMF language file '%s'!", path_c(&language_file2));
        goto error_0;
    }
    if(vector_size(&language2->strings) != LANG2_STR_COUNT) {
        log_error("Unable to load OpenOMF language file '%s', unsupported or corrupt file!", path_c(&language_file2));
        goto error_0;
    }

    log_info("Loaded OpenOMF language file '%s'.", path_c(&language_file2));

    return true;

error_0:
    lang_close();
    return false;
}

void lang_close(void) {
    sd_language_free(language);
    omf_free(language);
    sd_language_free(language2);
    omf_free(language2);
}

const char *lang_get(unsigned int id) {
    const sd_lang_string *entry = vector_get(&language->strings, id);
    if(entry == NULL || str_size(&entry->data) == 0) {
        log_error("unsupported lang id %u!", id);
        return "!INVALID!";
    }
    return str_c(&entry->data);
}

const char *lang_get2(unsigned int id) {
    const sd_lang_string *entry = vector_get(&language2->strings, id);
    if(entry == NULL || str_size(&entry->data) == 0) {
        log_error("unsupported lang2 id %u!", id);
        return "!INVALID2!";
    }
    return str_c(&entry->data);
}

// ---------------------------------------------------------------- KOREAN.UI
typedef struct ui_pair {
    char *en;
    char *ko;
} ui_pair;

static ui_pair *ui_pairs = NULL;
static size_t ui_count = 0;
static bool ui_loaded = false;

static int ui_cmp(const void *a, const void *b) {
    return strcmp(((const ui_pair *)a)->en, ((const ui_pair *)b)->en);
}

static void ui_unescape(char *t) {
    for(char *p = t; (p = strstr(p, "\\n")) != NULL;) {
        *p = '\n';
        memmove(p + 1, p + 2, strlen(p + 2) + 1);
    }
}

static void ui_load(void) {
    ui_loaded = true;
    const path filename = get_resource_filename("KOREAN.UI");
    FILE *f = fopen(path_c(&filename), "rb");
    if(f == NULL) {
        return;
    }
    char line[1024];
    size_t cap = 0;
    while(fgets(line, sizeof(line), f) != NULL) {
        size_t n = strlen(line);
        while(n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = 0;
        }
        char *tab = strchr(line, '\t');
        if(line[0] == '#' || tab == NULL || tab == line || tab[1] == 0) {
            continue;
        }
        *tab = 0;
        if(ui_count == cap) {
            cap = cap ? cap * 2 : 256;
            ui_pairs = omf_realloc(ui_pairs, cap * sizeof(ui_pair));
        }
        // "\n" in the file means a line break (both columns, as in the C source)
        char *en = omf_strdup(line);
        char *ko = omf_strdup(tab + 1);
        ui_unescape(en);
        ui_unescape(ko);
        ui_pairs[ui_count].en = en;
        ui_pairs[ui_count].ko = ko;
        ui_count++;
    }
    fclose(f);
    qsort(ui_pairs, ui_count, sizeof(ui_pair), ui_cmp);
    log_info("Loaded %u Korean UI strings from '%s'", (unsigned)ui_count, path_c(&filename));
}

// printf-style entries ("RANK: %d"): match the English format against an already formatted string,
// capture the inserted values and put them into the Korean format in the same order.
#define UI_MAX_CAPS 8
#define UI_CAP_LEN 64

static const char *spec_end(const char *f) {
    // f points after '%': flags, width, length modifiers, conversion letter
    while(*f && strchr("-+ #0123456789.lhzjt", *f)) {
        f++;
    }
    return *f ? f + 1 : f;
}

static bool ui_fmt_match(const char *fmt, const char *s, char caps[][UI_CAP_LEN], int *ncap) {
    *ncap = 0;
    while(*fmt) {
        if(*fmt == '%' && fmt[1] == '%') {
            if(*s != '%') {
                return false;
            }
            fmt += 2;
            s++;
            continue;
        }
        if(*fmt != '%') {
            if(*fmt != *s) {
                return false;
            }
            fmt++;
            s++;
            continue;
        }
        const char *end = spec_end(fmt + 1);
        char conv = end > fmt + 1 ? end[-1] : 0;
        const char *start = s;
        if(conv == 's') {
            // up to the next literal character of the format (or the end)
            char stop = *end == '%' ? 0 : *end;
            while(*s && *s != stop) {
                s++;
            }
        } else {
            if(*s == '-' || *s == '+') {
                s++;
            }
            while(*s && (isdigit((unsigned char)*s) || ((conv == 'f' || conv == 'g') && *s == '.'))) {
                s++;
            }
            if(s == start) {
                return false;
            }
        }
        if(*ncap >= UI_MAX_CAPS) {
            return false;
        }
        size_t n = (size_t)(s - start);
        if(n >= UI_CAP_LEN) {
            n = UI_CAP_LEN - 1;
        }
        memcpy(caps[*ncap], start, n);
        caps[*ncap][n] = 0;
        (*ncap)++;
        fmt = end;
    }
    return *s == 0;
}

static void ui_fmt_fill(char *out, size_t outlen, const char *ko, char caps[][UI_CAP_LEN], int ncap) {
    size_t o = 0;
    int k = 0;
    while(*ko && o + 1 < outlen) {
        if(*ko == '%' && ko[1] == '%') {
            out[o++] = '%';
            ko += 2;
        } else if(*ko == '%') {
            const char *cap = k < ncap ? caps[k] : "";
            k++;
            while(*cap && o + 1 < outlen) {
                out[o++] = *cap++;
            }
            ko = spec_end(ko + 1);
        } else {
            out[o++] = *ko++;
        }
    }
    out[o] = 0;
}

const char *lang_ui(const char *english) {
    if(english == NULL || !fonts_get_utf8()) {
        return english;
    }
    if(!ui_loaded) {
        ui_load();
    }
    if(ui_count == 0) {
        return english;
    }
    ui_pair key = {(char *)english, NULL};
    const ui_pair *hit = bsearch(&key, ui_pairs, ui_count, sizeof(ui_pair), ui_cmp);
    if(hit != NULL) {
        return hit->ko;
    }
    // formatted strings: try the entries that contain printf specifiers
    static char formatted[1024];
    char caps[UI_MAX_CAPS][UI_CAP_LEN];
    for(size_t i = 0; i < ui_count; i++) {
        if(strchr(ui_pairs[i].en, '%') == NULL) {
            continue;
        }
        int ncap = 0;
        if(ui_fmt_match(ui_pairs[i].en, english, caps, &ncap)) {
            ui_fmt_fill(formatted, sizeof(formatted), ui_pairs[i].ko, caps, ncap);
            return formatted;
        }
    }
    return english;
}

// ---------------------------------------------------------------- 한국어 조사
// 번역문에 "은(는)" 처럼 적어 두면, 앞 글자의 받침을 보고 알맞은 조사 하나로 바꾼다.
// (이름이 실행 중에 들어가는 뉴스·대사 문장용)
typedef struct josa_rule {
    const char *mark; // 번역문 표기
    const char *with; // 받침 있음
    const char *without;
} josa_rule;

static const josa_rule josa_rules[] = {
    {"은(는)", "은", "는"}, {"이(가)", "이", "가"},     {"을(를)", "을", "를"},   {"과(와)", "과", "와"},
    {"아(야)", "아", "야"}, {"이었(였)", "이었", "였"}, {"으로(로)", "으로", "로"}, {"이(여)", "이여", "여"},
};

// s[pos] 바로 앞 글자의 받침 종류: 0 없음, 1 있음, 2 ㄹ 받침 (한글이 아니면 숫자·영문 기준으로 대충)
static int josa_prev_final(const char *s, size_t pos) {
    if(pos == 0) {
        return 0;
    }
    size_t p = pos - 1;
    while(p > 0 && ((unsigned char)s[p] & 0xC0) == 0x80) {
        p--;
    }
    const unsigned char *u = (const unsigned char *)s + p;
    if(u[0] >= 0xE0 && u[0] < 0xF0 && pos - p == 3) {
        uint32_t cp = ((uint32_t)(u[0] & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) | (u[2] & 0x3F);
        if(cp >= 0xAC00 && cp <= 0xD7A3) {
            int jong = (int)((cp - 0xAC00) % 28);
            return jong == 0 ? 0 : (jong == 8 ? 2 : 1);
        }
    }
    // 숫자: 0136789 는 받침 있음(공·일·삼·육·칠·팔·구), 1·7·8 은 ㄹ
    char c = s[pos - 1];
    if(c >= '0' && c <= '9') {
        return (c == '1' || c == '7' || c == '8') ? 2 : ((c == '0' || c == '3' || c == '6') ? 1 : 0);
    }
    return 0;
}

void lang_josa(str *s) {
    if(!fonts_get_utf8()) {
        return;
    }
    for(size_t r = 0; r < sizeof(josa_rules) / sizeof(josa_rules[0]); r++) {
        const josa_rule *rule = &josa_rules[r];
        size_t pos = 0;
        while(true) {
            const char *base = str_c(s);
            const char *hit = strstr(base + pos, rule->mark);
            if(hit == NULL) {
                break;
            }
            size_t at = (size_t)(hit - base);
            int fin = josa_prev_final(base, at);
            const char *use = fin == 0 ? rule->without : rule->with;
            if(strcmp(rule->mark, "으로(로)") == 0 && fin == 2) {
                use = "로"; // ㄹ 받침 뒤에는 "로"
            }
            str tail;
            str_from_c(&tail, hit + strlen(rule->mark));
            str_truncate(s, at);
            str_append_c(s, use);
            pos = str_size(s);
            str_append(s, &tail);
            str_free(&tail);
        }
    }
}
