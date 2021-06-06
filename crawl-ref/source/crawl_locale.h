//
// Created by Аполлов Юрий Андреевич on 17.04.2021.
//

#ifndef SOURCE_CRAWL_LOCALE_H
#define SOURCE_CRAWL_LOCALE_H

#include <list>
#include <map>
#include <tuple>

#include "gettext.h"

// ########## Generic i18n functions ##########
// To be used only in simple situations, where both msgctxt and msgid are plain string literals.
// This way xgettext will find them and add to .pot file.
// In other situations, where there might be multiple contexts for each msgid, like with actors
//   or monster attack names, there should be specific enums and functions for handling, not to
//   mix them together. Also a careful collection of all the msgid's (terms) for those contexts
//   should be done in order to generate complete final .pot file for translators.
#define _(Msgid) gettext (Msgid)
#define __(Msgctxt, Msgid) strlen(Msgctxt) ? pgettext_expr (Msgctxt, Msgid) : gettext(Msgid)
#define pgettext_noop(Msgctxt, Msgid) Msgid


// ########## Actor-related i18n contexts ##########
#define ACTOR_I18N_CNAME const char* i18n_cname = __actor_i18n_cnames[i18n_context]

enum actor_i18n_context_type {
    I18NC_EMPTY,

    I18NC_MONSTER_MELEE_ATTACKER,
    I18NC_MONSTER_MELEE_DEFENDER,
    I18NC_PLAYER_MELEE_DEFENDER,
    I18NC_PLAYER_CONF_KILL_VICTIM,
    I18NC_PLAYER_KILL_VICTIM,
    I18NC_IOOD_ACT_ATTACKER,

    I18NCA_END_CONSTRICTION_ATTACKER,
    I18NCA_END_CONSTRICTION_DEFENDER,
    I18NCA_CONSTRICTION_DAMAGE_ATTACKER,
    I18NCA_CONSTRICTION_DAMAGE_DEFENDER,
    I18NCA_CONSTRICTION_DAMAGE_DEFENDER_PASSIVE,

    I18NCA_COLLIDE_ACTIVE,
    I18NCA_COLLIDE_PASSIVE,
    I18NCA_COLLIDE_DEATH_SOURCE,
    I18NCA_COLLIDE_DEATH_INVOKER,
    I18NCA_COLLIDE_INTO_FEATURE,
    I18NCA_COLLIDE_STOP_MOVEMENT,

    I18NCA_MELEE_ATTACK_DODGED_ATTACKER,
    I18NCA_MELEE_ATTACK_DODGED_DEFENDER,
    I18NCA_MELEE_ATTACK_MISSED_DEFENDER,

    I18NCA_TRAP_VICTIM,

    I18NCA_ART_FUNC_CAPTAIN_FLASHES_DEFENDER,
    I18NCA_ART_FUNC_CAPTAIN_WEAPON_FALLS_DEFENDER,

    ACTOR_I18NC_COUNT
};

// Changing these strings requires changing them in all the .po files' contexts
static const char* const __actor_i18n_cnames[ACTOR_I18NC_COUNT] = {
    "",

    "%s bites you for 10 damage with +3 dagger!!!",
    "The jackal bites %s for 10 damage with +3 dagger!!!",
    "You slice %s like an onion for 10 damage!!",
    "%s is blown up!",
    "You kill %s!",
    "%s hits a closed door.",

    "%s loses its grip on you.",
    "Octopode loses its grip on %s.",
    "%s constrict the jackal for 15, but do no damage.",
    "The grasping roots constrict %s for 15, but do no damage.",
    "%s is constricted for 15, but do no damage.",

    "%s collides with you!",
    "You collide with %s!",
    "Collided with %s caused by salamander mystic.",
    "Collided with the jackal caused by %s.",
    "%s slams into the wall!",
    "%s violently stops moving!",

    "%s completely misses centaur captain skeleton!",
    "The mighty blow completely misses %s!",
    "Your bite misses %s.",

    "A dart trap hits %s!",

    "The captain's cutlass flashes! You lacerate %s!!",
    "%s sword falls to the floor!",
};

const char* translate_actor(actor_i18n_context_type i18n_context, const char* msgid);

// ########## Monster attack name i18n contexts ##########
enum class mon_attack_name_i18n_ctype {
    mons_attack_verb,

    count
};

// Changing these strings requires changing them in all the .po files' contexts
static const char* const __mon_attack_name_i18n_cnames[static_cast<int>(mon_attack_name_i18n_ctype::count)] = {
    "The jackal %s you for 10 damage with +3 dagger!!!",
};

const char* translate_mon_attack_name(mon_attack_name_i18n_ctype i18n_context, const char* msgid);

// ########## Other i18n contexts ##########
#define OTHER_I18N_CNAME const char* i18n_cname = __other_i18n_cnames[i18n_context]

enum other_i18n_context_type {
    I18NC_MONSTER_ATTACK_DESC,
    I18NC_PLAYER_ATTACK_VERB,
    I18NC_PLAYER_ATTACK_DEGREE,
    I18NC_IOOD_ACT_DEFENDER,
    I18NC_SLAM_INTO,

    OTHER_I18NC_COUNT
};

// Changing these strings requires changing them in all the .po files' contexts
static const char* const __other_i18n_cnames[OTHER_I18NC_COUNT] = {
    "The jackal bites you for 10 damage%s!!!",
    "You %s the jackal like an onion for 10 damage!!",
    "You slice the jackal%s for 10 damage!!",
    "The orb of death hits %s.",
    "You slam into %s!",
};

const char* translate_other(other_i18n_context_type i18n_context, const char* msgid);

#endif //SOURCE_CRAWL_LOCALE_H
