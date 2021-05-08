//
// Created by Аполлов Юрий Андреевич on 17.04.2021.
//

#ifndef SOURCE_CRAWL_LOCALE_H
#define SOURCE_CRAWL_LOCALE_H

#include "gettext.h"

// ########## Generic i18n functions ##########
#define _(Msgid) gettext (Msgid)
#define __(Msgctxt, Msgid) strlen(Msgctxt) ? pgettext_expr (Msgctxt, Msgid) : gettext(Msgid)


// ########## Actor-related i18n contexts ##########
#define ACTOR_I18N_CNAME const char* i18n_cname = actor_i18n_cnames[i18n_context]

enum actor_i18n_context_type {
    I18NC_EMPTY,

    I18NC_MONSTER_MELEE_ATTACKER,
    I18NC_MONSTER_MELEE_DEFENDER,
    I18NC_PLAYER_MELEE_DEFENDER,
    I18NC_PLAYER_CONF_KILL_VICTIM,
    I18NC_PLAYER_KILL_VICTIM,
    I18NC_IOOD_ACT_ATTACKER,

    I18NC_COUNT
};

// Changing these strings requires changing them in all the .po files' contexts
static const char* const actor_i18n_cnames[I18NC_COUNT] = {
    "",

    "%s bites you for 10 damage with +3 dagger!!!",
    "The jackal bites %s for 10 damage with +3 dagger!!!",
    "You slice %s like an onion for 10 damage!!",
    "%s is blown up!",
    "You kill %s!",
    "%s hits a closed door." ,
};

const char* translate_actor(actor_i18n_context_type i18n_context, const char* msgid);

// ########## Other i18n contexts##########
#define OTHER_I18N_CNAME const char* i18n_cname = other_i18n_cnames[i18n_context]

enum other_i18n_context_type {
    I18NC_MONSTER_MELEE_ATTACK_VERB,
    I18NC_MONSTER_ATTACK_DESC,
    I18NC_PLAYER_ATTACK_VERB,
    I18NC_PLAYER_ATTACK_DEGREE,
    I18NC_PLAYER_CONF_KILL_TYPE,
    I18NC_PLAYER_KILL_TYPE,
    I18NC_IOOD_ACT_DEFENDER,

    OTHER_I18NC_COUNT,
};

// Changing these strings requires changing them in all the .po files' contexts
static const char* const other_i18n_cnames[OTHER_I18NC_COUNT] = {
    "The jackal %s you for 10 damage with +3 dagger!!!",
    "The jackal bites you for 10 damage%s!!!",
    "You %s the jackal like an onion for 10 damage!!",
    "You slice the jackal%s for 10 damage!!",
    "The jackal is %s!",
    "You %s a jackal!",
    "The orb of death hits %s.",
};

const char* translate_other(other_i18n_context_type i18n_context, const char* msgid);

#endif //SOURCE_CRAWL_LOCALE_H
