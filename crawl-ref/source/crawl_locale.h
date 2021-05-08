//
// Created by Аполлов Юрий Андреевич on 17.04.2021.
//

#ifndef SOURCE_CRAWL_LOCALE_H
#define SOURCE_CRAWL_LOCALE_H

#include "gettext.h"

#define _(Msgid) gettext (Msgid)
#define __(Msgctxt, Msgid) strlen(Msgctxt) ? pgettext_expr (Msgctxt, Msgid) : gettext(Msgid)
#define I18N_CONTEXT_NAME const char* i18n_cname = i18n_cnames[i18n_context]
#define I18(Mcontext, Msgid) __(i18n_cnames[Mcontext], Msgid)

enum i18n_context_type {
    I18NC_EMPTY,

    I18NC_MONSTER_MELEE_ATTACKER,
    I18NC_MONSTER_MELEE_ATTACK_VERB,
    I18NC_MONSTER_MELEE_DEFENDER,
    I18NC_MONSTER_ATTACK_DESC,

    I18NC_PLAYER_ATTACK_VERB,
    I18NC_PLAYER_MELEE_DEFENDER,
    I18NC_PLAYER_ATTACK_DEGREE,

    I18NC_PLAYER_CONF_KILL_VICTIM,
    I18NC_PLAYER_CONF_KILL_TYPE,

    I18NC_PLAYER_KILL_TYPE,
    I18NC_PLAYER_KILL_VICTIM,

    I18NC_IOOD_ACT_ATTACKER,
    I18NC_IOOD_ACT_DEFENDER,

    I18NC_COUNT
};

// Changing these strings requires changing them in all the .po files' contexts
static const char* const i18n_cnames[I18NC_COUNT] = {
    "",

    "%s bites you for 10 damage with +3 dagger!!!",
    "The jackal %s you for 10 damage with +3 dagger!!!",
    "The jackal bites %s for 10 damage with +3 dagger!!!",
    "The jackal bites you for 10 damage%s!!!",

    "You %s the jackal like an onion for 10 damage!!!!!",
    "You slice %s like an onion for 10 damage!!!!!",
    "You slice the jackal%s for 10 damage!!!!!",

    "%s is %(blown up)s!",
    "The jackal is %s!",

    "You %s %(a jackal)s!",
    "You kill %(a jackal)s!",

    "%s hits a closed door." ,
    "The orb of death hits %s.",
};

#endif //SOURCE_CRAWL_LOCALE_H
