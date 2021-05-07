//
// Created by Аполлов Юрий Андреевич on 17.04.2021.
//

#ifndef SOURCE_CRAWL_LOCALE_H
#define SOURCE_CRAWL_LOCALE_H

#include "gettext.h"

#define _(Msgid) gettext (Msgid)
#define __(Msgctxt, Msgid) strlen(Msgctxt) ? pgettext_expr (Msgctxt, Msgid) : gettext(Msgid)
#define I18N_CONTEXT_NAME const char* i18n_cname = i18n_cnames[i18n_context].c_str()
#define I18(Mcontext, Msgid) __(i18n_cnames[Mcontext].c_str(), Msgid)

enum i18n_context {
    I18NC_EMPTY,
    I18NC_MELEE_ATTACKER,
    I18NC_MELEE_ATTACK_VERB
};

// Changing these strings requires changing them in all the .po files' contexts
static string i18n_cnames[] = {
    "",
    "%s bites you for 10 damage with +3 dagger!!!",
    "The jackal %s you for 10 damage with +3 dagger!!!"
};

#endif //SOURCE_CRAWL_LOCALE_H
