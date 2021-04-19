//
// Created by Аполлов Юрий Андреевич on 17.04.2021.
//

#ifndef SOURCE_CRAWL_LOCALE_H
#define SOURCE_CRAWL_LOCALE_H

#include "gettext.h"
#define _(Msgid) gettext (Msgid)
#define __(Msgctxt, Msgid) pgettext_expr (Msgctxt, Msgid)

#endif //SOURCE_CRAWL_LOCALE_H
