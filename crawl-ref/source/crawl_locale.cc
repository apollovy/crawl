//
// Created by Аполлов Юрий Андреевич on 17.04.2021.
//

#ifndef SOURCE_CRAWL_LOCALE_CC
#define SOURCE_CRAWL_LOCALE_CC

#include "crawl_locale.h"

const char* translate_actor(actor_i18n_context_type i18n_context, const char* msgid) {
    return __(actor_i18n_cnames[i18n_context], msgid);
}

const char* translate_other(other_i18n_context_type i18n_context, const char *msgid) {
    return __(other_i18n_cnames[i18n_context], msgid);
}

#endif //SOURCE_CRAWL_LOCALE_CC
