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
        // "\n" in the file means a line break
        char *ko = omf_strdup(tab + 1);
        for(char *p = ko; (p = strstr(p, "\\n")) != NULL;) {
            *p = '\n';
            memmove(p + 1, p + 2, strlen(p + 2) + 1);
        }
        ui_pairs[ui_count].en = omf_strdup(line);
        ui_pairs[ui_count].ko = ko;
        ui_count++;
    }
    fclose(f);
    qsort(ui_pairs, ui_count, sizeof(ui_pair), ui_cmp);
    log_info("Loaded %u Korean UI strings from '%s'", (unsigned)ui_count, path_c(&filename));
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
    return hit != NULL ? hit->ko : english;
}
