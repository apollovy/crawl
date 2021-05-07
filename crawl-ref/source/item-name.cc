/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "item-name.h"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "decks.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h" // LSTATE_STILL_WINDS
#include "errors.h" // sysfail
#include "god-item.h"
#include "god-passive.h" // passive_t::want_curses, no_haste
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "level-state-type.h"
#include "libutil.h"
#include "makeitem.h"
#include "notes.h"
#include "options.h"
#include "orb-type.h"
#include "player.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-goditem.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "throw.h"
#include "transform.h"
#include "unicode.h"
#include "unwind.h"
#include "viewgeom.h"
#include "crawl_locale.h"

static bool _is_consonant(char let);
static char _random_vowel();
static char _random_cons();
static string _random_consonant_set(size_t seed);

static void _maybe_identify_pack_item()
{
    for (auto &item : you.inv)
        if (item.defined() && !get_ident_type(item))
            maybe_identify_base_type(item);
}

// quant_name is useful since it prints out a different number of items
// than the item actually contains.
string quant_name(const item_def &item, int quant,
                  description_level_type des, bool terse)
{
    // item_name now requires a "real" item, so we'll mangle a tmp
    item_def tmp = item;
    tmp.quantity = quant;

    return tmp.name(des, terse);
}

static const char* _interesting_origin(const item_def &item)
{
    if (origin_as_god_gift(item) != GOD_NO_GOD)
        return gettext_noop("god gift");

    if (item.orig_monnum == MONS_DONALD && get_equip_desc(item)
        && item.is_type(OBJ_ARMOUR, ARM_KITE_SHIELD))
    {
        return gettext_noop("Donald");
    }

    return nullptr;
}

/**
 * What inscription should be appended to the given item's name?
 */
static string _item_inscription(const item_def &item)
{
    vector<string> insparts;

    if (const char *orig = _interesting_origin(item))
    {
        if (Options.show_god_gift == MB_TRUE
            || Options.show_god_gift == MB_MAYBE && !fully_identified(item))
        {
            insparts.push_back(orig);
        }
    }

    if (is_artefact(item))
    {
        const string part = artefact_inscription(item);
        if (!part.empty())
            insparts.push_back(part);
    }

    if (!item.inscription.empty())
        insparts.push_back(item.inscription);

    if (insparts.empty())
        return "";

    return make_stringf(gettext_noop(" {%s}"),
                        comma_separated_line(begin(insparts),
                                             end(insparts),
                                             ", ").c_str());
}

string item_def::name(description_level_type descrip, bool terse, bool ident,
                      bool with_inscription, bool quantity_in_words,
                      iflags_t ignore_flags) const
{
    if (crawl_state.game_is_arena())
        ignore_flags |= ISFLAG_KNOW_PLUSES | ISFLAG_COSMETIC_MASK;

    if (descrip == DESC_NONE)
        return "";

    ostringstream buff;

    const string auxname = name_aux(descrip, terse, ident, with_inscription,
                                    ignore_flags);

    const bool startvowel     = is_vowel(auxname[0]);
    const bool qualname       = (descrip == DESC_QUALNAME);

    if (descrip == DESC_INVENTORY_EQUIP || descrip == DESC_INVENTORY)
    {
        if (in_inventory(*this)) // actually in inventory
        {
            buff << index_to_letter(link);
            if (terse)
                buff << ") ";
            else
                buff << " - ";
        }
        else
            descrip = DESC_A;
    }

    if (base_type == OBJ_BOOKS && (ident || item_type_known(*this))
        && book_has_title(*this))
    {
        if (descrip != DESC_DBNAME)
            descrip = DESC_PLAIN;
    }

    if (terse && descrip != DESC_DBNAME)
        descrip = DESC_PLAIN;

    monster_flags_t corpse_flags;

    // no "a dragon scales"
    const bool always_plural = armour_is_hide(*this)
                               && sub_type != ARM_TROLL_LEATHER_ARMOUR;

    if ((base_type == OBJ_CORPSES && is_named_corpse(*this)
         && !(((corpse_flags.flags = props[CORPSE_NAME_TYPE_KEY].get_int64())
               & MF_NAME_SPECIES)
              && !(corpse_flags & MF_NAME_DEFINITE))
         && !(corpse_flags & MF_NAME_SUFFIX)
         && !starts_with(get_corpse_name(*this), gettext_noop("shaped ")))
        || item_is_orb(*this) || item_is_horn_of_geryon(*this)
        || (ident || item_type_known(*this)) && is_artefact(*this)
            && special != UNRAND_OCTOPUS_KING_RING)
    {
        // Artefacts always get "the" unless we just want the plain name.
        switch (descrip)
        {
        default:
            buff << "the ";
        case DESC_PLAIN:
        case DESC_DBNAME:
        case DESC_BASENAME:
        case DESC_QUALNAME:
            break;
        }
    }
    else if (quantity > 1 || always_plural)
    {
        switch (descrip)
        {
        case DESC_THE:        buff << "the "; break;
        case DESC_YOUR:       buff << "your "; break;
        case DESC_ITS:        buff << "its "; break;
        case DESC_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
        case DESC_PLAIN:
        default:
            break;
        }

        if (descrip != DESC_BASENAME && descrip != DESC_QUALNAME
            && descrip != DESC_DBNAME && !always_plural)
        {
            if (quantity_in_words)
                buff << number_in_words(quantity) << " ";
            else
                buff << quantity << " ";
        }
    }
    else
    {
        switch (descrip)
        {
        case DESC_THE:        buff << "the "; break;
        case DESC_YOUR:       buff << "your "; break;
        case DESC_ITS:        buff << "its "; break;
        case DESC_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
                              buff << (startvowel ? "an " : "a "); break;
        case DESC_PLAIN:
        default:
            break;
        }
    }

    buff << auxname;

    if (descrip == DESC_INVENTORY_EQUIP)
    {
        equipment_type eq = item_equip_slot(*this);
        if (eq != EQ_NONE)
        {
            if (you.melded[eq])
                buff << gettext_noop(" (melded)");
            else
            {
                switch (eq)
                {
                case EQ_WEAPON:
                    if (is_weapon(*this))
                        buff << gettext_noop(" (weapon)");
                    else if (you.has_mutation(MUT_NO_GRASPING))
                        buff << gettext_noop(" (in mouth)");
                    else
                        buff << " (in " << you.hand_name(false) << ")";
                    break;
                case EQ_CLOAK:
                case EQ_HELMET:
                case EQ_GLOVES:
                case EQ_BOOTS:
                case EQ_SHIELD:
                case EQ_BODY_ARMOUR:
                    buff << gettext_noop(" (worn)");
                    break;
                case EQ_LEFT_RING:
                case EQ_RIGHT_RING:
                case EQ_RING_ONE:
                case EQ_RING_TWO:
                    buff << " (";
                    buff << ((eq == EQ_LEFT_RING || eq == EQ_RING_ONE)
                             ? "left" : "right");
                    buff << " ";
                    buff << you.hand_name(false);
                    buff << ")";
                    break;
                case EQ_AMULET:
                    if (you.species == SP_OCTOPODE && form_keeps_mutations())
                        buff << gettext_noop(" (around mantle)");
                    else
                        buff << gettext_noop(" (around neck)");
                    break;
                case EQ_RING_THREE:
                case EQ_RING_FOUR:
                case EQ_RING_FIVE:
                case EQ_RING_SIX:
                case EQ_RING_SEVEN:
                case EQ_RING_EIGHT:
                    buff << gettext_noop(" (on tentacle)");
                    break;
                case EQ_RING_AMULET:
                    buff << gettext_noop(" (on amulet)");
                    break;
                default:
                    die("Item in an invalid slot");
                }
            }
        }
        else if (you.launcher_action.item_is_quivered(*this))
            buff << gettext_noop(" (quivered ammo)");
        else if (you.quiver_action.item_is_quivered(*this))
            buff << gettext_noop(" (quivered)");
    }

    if (descrip != DESC_BASENAME && descrip != DESC_DBNAME && with_inscription)
        buff << _item_inscription(*this);

    // These didn't have "cursed " prepended; add them here so that
    // it comes after the inscription.
    if (terse && descrip != DESC_DBNAME && descrip != DESC_BASENAME
        && !qualname
        && is_artefact(*this) && cursed())
    {
        buff << gettext_noop(" (curse)");
    }

    return buff.str();
}

static bool _missile_brand_is_prefix(special_missile_type brand)
{
    switch (brand)
    {
    case SPMSL_POISONED:
    case SPMSL_CURARE:
    case SPMSL_BLINDING:
    case SPMSL_FRENZY:
    case SPMSL_EXPLODING:
#if TAG_MAJOR_VERSION == 34
    case SPMSL_STEEL:
#endif
    case SPMSL_SILVER:
        return true;
    default:
        return false;
    }
}

static bool _missile_brand_is_postfix(special_missile_type brand)
{
    return brand != SPMSL_NORMAL && !_missile_brand_is_prefix(brand);
}

const char* missile_brand_name(const item_def &item, mbn_type t)
{
    const special_missile_type brand
        = static_cast<special_missile_type>(item.brand);
    switch (brand)
    {
#if TAG_MAJOR_VERSION == 34
    case SPMSL_FLAME:
        return gettext_noop("flame");
    case SPMSL_FROST:
        return gettext_noop("frost");
#endif
    case SPMSL_POISONED:
        return t == MBN_NAME ? gettext_noop("poisoned") : gettext_noop("poison");
    case SPMSL_CURARE:
        return t == MBN_NAME ? gettext_noop("curare-tipped") : gettext_noop("curare");
#if TAG_MAJOR_VERSION == 34
    case SPMSL_EXPLODING:
        return t == MBN_TERSE ? gettext_noop("explode") : gettext_noop("exploding");
    case SPMSL_STEEL:
        return gettext_noop("steel");
    case SPMSL_RETURNING:
        return t == MBN_TERSE ? gettext_noop("return") : gettext_noop("returning");
    case SPMSL_PENETRATION:
        return t == MBN_TERSE ? gettext_noop("penet") : gettext_noop("penetration");
#endif
    case SPMSL_SILVER:
        return gettext_noop("silver");
#if TAG_MAJOR_VERSION == 34
    case SPMSL_PARALYSIS:
        return gettext_noop("paralysis");
    case SPMSL_SLOW:
        return t == MBN_TERSE ? gettext_noop("slow") : gettext_noop("slowing");
    case SPMSL_SLEEP:
        return t == MBN_TERSE ? gettext_noop("sleep") : gettext_noop("sleeping");
    case SPMSL_CONFUSION:
        return t == MBN_TERSE ? gettext_noop("conf") : gettext_noop("confusion");
    case SPMSL_SICKNESS:
        return t == MBN_TERSE ? gettext_noop("sick") : gettext_noop("sickness");
#endif
    case SPMSL_FRENZY:
        return t == MBN_NAME ? gettext_noop("datura-tipped") : gettext_noop("datura");
    case SPMSL_CHAOS:
        return gettext_noop("chaos");
    case SPMSL_DISPERSAL:
        return t == MBN_TERSE ? gettext_noop("disperse") : gettext_noop("dispersal");
    case SPMSL_BLINDING:
        return t == MBN_NAME ? gettext_noop("atropa-tipped") : gettext_noop("atropa");
    case SPMSL_NORMAL:
        return "";
    default:
        return t == MBN_TERSE ? gettext_noop("buggy") : gettext_noop("bugginess");
    }
}

static const char *weapon_brands_terse[] =
{
    "", gettext_noop("flame"), gettext_noop("freeze"), gettext_noop("holy"), gettext_noop("elec"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("obsolete"), gettext_noop("obsolete"),
#endif
    gettext_noop("venom"), gettext_noop("protect"), gettext_noop("drain"), gettext_noop("speed"), gettext_noop("vorpal"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("obsolete"), gettext_noop("obsolete"),
#endif
    gettext_noop("vamp"), gettext_noop("pain"), gettext_noop("antimagic"), gettext_noop("distort"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("obsolete"), gettext_noop("obsolete"),
#endif
    gettext_noop("chaos"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("evade"), gettext_noop("confuse"),
#endif
    gettext_noop("penet"), gettext_noop("reap"), gettext_noop("spect"), gettext_noop("vorpal"), gettext_noop("acid"),
#if TAG_MAJOR_VERSION > 34
    gettext_noop("confuse"),
#endif
    gettext_noop("debug"),
};

static const char *weapon_brands_verbose[] =
{
    "", gettext_noop("flaming"), gettext_noop("freezing"), gettext_noop("holy wrath"), gettext_noop("electrocution"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("orc slaying"), gettext_noop("dragon slaying"),
#endif
    gettext_noop("venom"), gettext_noop("protection"), gettext_noop("draining"), gettext_noop("speed"), gettext_noop("vorpality"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("flame"), gettext_noop("frost"),
#endif
    gettext_noop("vampirism"), gettext_noop("pain"), gettext_noop("antimagic"), gettext_noop("distortion"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("reaching"), gettext_noop("returning"),
#endif
    gettext_noop("chaos"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("evasion"), gettext_noop("confusion"),
#endif
    gettext_noop("penetration"), gettext_noop("reaping"), gettext_noop("spectralizing"), gettext_noop("vorpal"), gettext_noop("acid"),
#if TAG_MAJOR_VERSION > 34
    gettext_noop("confusion"),
#endif
    gettext_noop("debug"),
};

static const char *weapon_brands_adj[] =
{
    "", gettext_noop("flaming"), gettext_noop("freezing"), gettext_noop("holy"), gettext_noop("electric"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("orc-killing"), gettext_noop("dragon-slaying"),
#endif
    gettext_noop("venomous"), gettext_noop("protective"), gettext_noop("draining"), gettext_noop("fast"), gettext_noop("vorpal"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("flaming"), gettext_noop("freezing"),
#endif
    gettext_noop("vampiric"), gettext_noop("painful"), gettext_noop("antimagic"), gettext_noop("distorting"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("reaching"), gettext_noop("returning"),
#endif
    gettext_noop("chaotic"),
#if TAG_MAJOR_VERSION == 34
    gettext_noop("evasive"), gettext_noop("confusing"),
#endif
    gettext_noop("penetrating"), gettext_noop("reaping"), gettext_noop("spectral"), gettext_noop("vorpal"), gettext_noop("acidic"),
#if TAG_MAJOR_VERSION > 34
    gettext_noop("confusing"),
#endif
    gettext_noop("debug"),
};

static const set<brand_type> brand_prefers_adj =
            { SPWPN_VAMPIRISM, SPWPN_ANTIMAGIC, SPWPN_VORPAL, SPWPN_SPECTRAL };

/**
 * What's the name of a type of weapon brand?
 *
 * @param brand             The type of brand in question.
 * @param bool              Whether to use a terse or verbose name.
 * @return                  The name of the given brand.
 */
const char* brand_type_name(brand_type brand, bool terse)
{
    COMPILE_CHECK(ARRAYSZ(weapon_brands_terse) == NUM_SPECIAL_WEAPONS);
    COMPILE_CHECK(ARRAYSZ(weapon_brands_verbose) == NUM_SPECIAL_WEAPONS);

    if (brand < 0 || brand >= NUM_SPECIAL_WEAPONS)
        return terse ? gettext_noop("buggy") : gettext_noop("bugginess");

    return (terse ? weapon_brands_terse : weapon_brands_verbose)[brand];
}

const char* brand_type_adj(brand_type brand)
{
    COMPILE_CHECK(ARRAYSZ(weapon_brands_terse) == NUM_SPECIAL_WEAPONS);
    COMPILE_CHECK(ARRAYSZ(weapon_brands_verbose) == NUM_SPECIAL_WEAPONS);

    if (brand < 0 || brand >= NUM_SPECIAL_WEAPONS)
        return gettext_noop("buggy");

    return weapon_brands_adj[brand];
}

/**
 * What's the name of a given weapon's brand?
 *
 * @param item              The weapon with the brand.
 * @param bool              Whether to use a terse or verbose name.
 * @return                  The name of the given item's brand.
 */
const char* weapon_brand_name(const item_def& item, bool terse,
                              brand_type override_brand)
{
    const brand_type brand = override_brand ? override_brand : get_weapon_brand(item);

    return brand_type_name(brand, terse);
}

const char* armour_ego_name(const item_def& item, bool terse)
{
    if (!terse)
    {
        switch (get_armour_ego_type(item))
        {
        case SPARM_NORMAL:            return "";
#if TAG_MAJOR_VERSION == 34
        case SPARM_RUNNING:           return gettext_noop("running");
#endif
        case SPARM_FIRE_RESISTANCE:   return gettext_noop("fire resistance");
        case SPARM_COLD_RESISTANCE:   return gettext_noop("cold resistance");
        case SPARM_POISON_RESISTANCE: return gettext_noop("poison resistance");
        case SPARM_SEE_INVISIBLE:     return gettext_noop("see invisible");
        case SPARM_INVISIBILITY:      return gettext_noop("invisibility");
        case SPARM_STRENGTH:          return gettext_noop("strength");
        case SPARM_DEXTERITY:         return gettext_noop("dexterity");
        case SPARM_INTELLIGENCE:      return gettext_noop("intelligence");
        case SPARM_PONDEROUSNESS:     return gettext_noop("ponderousness");
        case SPARM_FLYING:            return gettext_noop("flying");

        case SPARM_WILLPOWER:         return gettext_noop("willpower");
        case SPARM_PROTECTION:        return gettext_noop("protection");
        case SPARM_STEALTH:           return gettext_noop("stealth");
        case SPARM_RESISTANCE:        return gettext_noop("resistance");
        case SPARM_POSITIVE_ENERGY:   return gettext_noop("positive energy");
        case SPARM_ARCHMAGI:          return gettext_noop("the Archmagi");
#if TAG_MAJOR_VERSION == 34
        case SPARM_JUMPING:           return gettext_noop("jumping");
#endif
        case SPARM_PRESERVATION:      return gettext_noop("preservation");
        case SPARM_REFLECTION:        return gettext_noop("reflection");
        case SPARM_SPIRIT_SHIELD:     return gettext_noop("spirit shield");
        case SPARM_ARCHERY:           return gettext_noop("archery");
        case SPARM_REPULSION:         return gettext_noop("repulsion");
#if TAG_MAJOR_VERSION == 34
        case SPARM_CLOUD_IMMUNE:      return gettext_noop("cloud immunity");
#endif
        case SPARM_HARM:              return gettext_noop("harm");
        case SPARM_SHADOWS:           return gettext_noop("shadows");
        case SPARM_RAMPAGING:         return gettext_noop("rampaging");
        default:                      return gettext_noop("bugginess");
        }
    }
    else
    {
        switch (get_armour_ego_type(item))
        {
        case SPARM_NORMAL:            return "";
#if TAG_MAJOR_VERSION == 34
        case SPARM_RUNNING:           return gettext_noop("obsolete");
#endif
        case SPARM_FIRE_RESISTANCE:   return gettext_noop("rF+");
        case SPARM_COLD_RESISTANCE:   return gettext_noop("rC+");
        case SPARM_POISON_RESISTANCE: return gettext_noop("rPois");
        case SPARM_SEE_INVISIBLE:     return gettext_noop("SInv");
        case SPARM_INVISIBILITY:      return gettext_noop("+Inv");
        case SPARM_STRENGTH:          return gettext_noop("Str+3");
        case SPARM_DEXTERITY:         return gettext_noop("Dex+3");
        case SPARM_INTELLIGENCE:      return gettext_noop("Int+3");
        case SPARM_PONDEROUSNESS:     return gettext_noop("ponderous");
        case SPARM_FLYING:            return gettext_noop("Fly");
        case SPARM_WILLPOWER:         return gettext_noop("Will+");
        case SPARM_PROTECTION:        return gettext_noop("AC+3");
        case SPARM_STEALTH:           return gettext_noop("Stlth+");
        case SPARM_RESISTANCE:        return gettext_noop("rC+ rF+");
        case SPARM_POSITIVE_ENERGY:   return gettext_noop("rN+");
        case SPARM_ARCHMAGI:          return gettext_noop("Archmagi");
#if TAG_MAJOR_VERSION == 34
        case SPARM_JUMPING:           return gettext_noop("obsolete");
#endif
        case SPARM_PRESERVATION:      return gettext_noop("rCorr");
        case SPARM_REFLECTION:        return gettext_noop("reflect");
        case SPARM_SPIRIT_SHIELD:     return gettext_noop("Spirit");
        case SPARM_ARCHERY:           return gettext_noop("archery");
        case SPARM_REPULSION:         return gettext_noop("repulsion");
#if TAG_MAJOR_VERSION == 34
        case SPARM_CLOUD_IMMUNE:      return gettext_noop("obsolete");
#endif
        case SPARM_HARM:              return gettext_noop("harm");
        case SPARM_SHADOWS:           return gettext_noop("shadows");
        case SPARM_RAMPAGING:         return gettext_noop("rampage");
        default:                      return gettext_noop("buggy");
        }
    }
}

static const char* _wand_type_name(int wandtype)
{
    switch (wandtype)
    {
    case WAND_FLAME:           return gettext_noop("flame");
    case WAND_PARALYSIS:       return gettext_noop("paralysis");
    case WAND_DIGGING:         return gettext_noop("digging");
    case WAND_ICEBLAST:        return gettext_noop("iceblast");
    case WAND_POLYMORPH:       return gettext_noop("polymorph");
    case WAND_CHARMING:     return gettext_noop("charming");
    case WAND_ACID:            return gettext_noop("acid");
    case WAND_MINDBURST:       return gettext_noop("mindburst");
    default:                   return item_type_removed(OBJ_WANDS, wandtype)
                                    ? gettext_noop("removedness")
                                    : gettext_noop("bugginess");
    }
}

static const char* wand_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        "", gettext_noop("jewelled "), gettext_noop("curved "), gettext_noop("long "), gettext_noop("short "), gettext_noop("twisted "), gettext_noop("crooked "),
        gettext_noop("forked "), gettext_noop("shiny "), gettext_noop("blackened "), gettext_noop("tapered "), gettext_noop("glowing "), gettext_noop("worn "),
        gettext_noop("encrusted "), gettext_noop("runed "), gettext_noop("sharpened ")
    };
    COMPILE_CHECK(ARRAYSZ(secondary_strings) == NDSC_WAND_SEC);
    return secondary_strings[s % NDSC_WAND_SEC];
}

static const char* wand_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        gettext_noop("iron"), gettext_noop("brass"), gettext_noop("bone"), gettext_noop("wooden"), gettext_noop("copper"), gettext_noop("gold"), gettext_noop("silver"),
        gettext_noop("bronze"), gettext_noop("ivory"), gettext_noop("glass"), gettext_noop("lead"), gettext_noop("fluorescent")
    };
    COMPILE_CHECK(ARRAYSZ(primary_strings) == NDSC_WAND_PRI);
    return primary_strings[p % NDSC_WAND_PRI];
}

const char* potion_type_name(int potiontype)
{
    switch (static_cast<potion_type>(potiontype))
    {
    case POT_CURING:            return gettext_noop("curing");
    case POT_HEAL_WOUNDS:       return gettext_noop("heal wounds");
    case POT_HASTE:             return gettext_noop("haste");
    case POT_MIGHT:             return gettext_noop("might");
    case POT_ATTRACTION:        return gettext_noop("attraction");
    case POT_BRILLIANCE:        return gettext_noop("brilliance");
    case POT_FLIGHT:            return gettext_noop("flight");
    case POT_CANCELLATION:      return gettext_noop("cancellation");
    case POT_AMBROSIA:          return gettext_noop("ambrosia");
    case POT_INVISIBILITY:      return gettext_noop("invisibility");
    case POT_DEGENERATION:      return gettext_noop("degeneration");
    case POT_EXPERIENCE:        return gettext_noop("experience");
    case POT_MAGIC:             return gettext_noop("magic");
    case POT_BERSERK_RAGE:      return gettext_noop("berserk rage");
    case POT_MUTATION:          return gettext_noop("mutation");
    case POT_RESISTANCE:        return gettext_noop("resistance");
    case POT_LIGNIFY:           return gettext_noop("lignification");

    // FIXME: Remove this once known-items no longer uses this as a sentinel.
    default:
                                return gettext_noop("bugginess");
    CASE_REMOVED_POTIONS(potiontype); // TODO: this will crash, is that correct??
    }
}

static const char* scroll_type_name(int scrolltype)
{
    switch (static_cast<scroll_type>(scrolltype))
    {
    case SCR_IDENTIFY:           return gettext_noop("identify");
    case SCR_TELEPORTATION:      return gettext_noop("teleportation");
    case SCR_FEAR:               return gettext_noop("fear");
    case SCR_NOISE:              return gettext_noop("noise");
    case SCR_SUMMONING:          return gettext_noop("summoning");
    case SCR_ENCHANT_WEAPON:     return gettext_noop("enchant weapon");
    case SCR_ENCHANT_ARMOUR:     return gettext_noop("enchant armour");
    case SCR_TORMENT:            return gettext_noop("torment");
    case SCR_IMMOLATION:         return gettext_noop("immolation");
    case SCR_BLINKING:           return gettext_noop("blinking");
    case SCR_MAGIC_MAPPING:      return gettext_noop("magic mapping");
    case SCR_FOG:                return gettext_noop("fog");
    case SCR_ACQUIREMENT:        return gettext_noop("acquirement");
    case SCR_BRAND_WEAPON:       return gettext_noop("brand weapon");
    case SCR_HOLY_WORD:          return gettext_noop("holy word");
    case SCR_VULNERABILITY:      return gettext_noop("vulnerability");
    case SCR_SILENCE:            return gettext_noop("silence");
    case SCR_AMNESIA:            return gettext_noop("amnesia");
#if TAG_MAJOR_VERSION == 34
    case SCR_CURSE_WEAPON:       return gettext_noop("curse weapon");
    case SCR_CURSE_ARMOUR:       return gettext_noop("curse armour");
    case SCR_CURSE_JEWELLERY:    return gettext_noop("curse jewellery");
#endif
    default:                     return item_type_removed(OBJ_SCROLLS,
                                                          scrolltype)
                                     ? gettext_noop("removedness")
                                     : gettext_noop("bugginess");
    }
}

/**
 * Get the name for the effect provided by a kind of jewellery.
 *
 * @param jeweltype     The jewellery_type of the item in question.
 * @return              A string describing the effect of the given jewellery
 *                      subtype.
 */
const char* jewellery_effect_name(int jeweltype, bool terse)
{
    if (!terse)
    {
        switch (static_cast<jewellery_type>(jeweltype))
        {
#if TAG_MAJOR_VERSION == 34
        case RING_REGENERATION:          return gettext_noop("obsoleteness");
        case RING_ATTENTION:             return gettext_noop("obsoleteness");
#endif
        case RING_PROTECTION:            return gettext_noop("protection");
        case RING_PROTECTION_FROM_FIRE:  return gettext_noop("protection from fire");
        case RING_POISON_RESISTANCE:     return gettext_noop("poison resistance");
        case RING_PROTECTION_FROM_COLD:  return gettext_noop("protection from cold");
        case RING_STRENGTH:              return gettext_noop("strength");
        case RING_SLAYING:               return gettext_noop("slaying");
        case RING_SEE_INVISIBLE:         return gettext_noop("see invisible");
        case RING_RESIST_CORROSION:      return gettext_noop("resist corrosion");
        case RING_EVASION:               return gettext_noop("evasion");
#if TAG_MAJOR_VERSION == 34
        case RING_SUSTAIN_ATTRIBUTES:    return gettext_noop("sustain attributes");
#endif
        case RING_STEALTH:               return gettext_noop("stealth");
        case RING_DEXTERITY:             return gettext_noop("dexterity");
        case RING_INTELLIGENCE:          return gettext_noop("intelligence");
        case RING_WIZARDRY:              return gettext_noop("wizardry");
        case RING_MAGICAL_POWER:         return gettext_noop("magical power");
        case RING_FLIGHT:                return gettext_noop("flight");
        case RING_LIFE_PROTECTION:       return gettext_noop("positive energy");
        case RING_WILLPOWER: return gettext_noop("willpower");
        case RING_FIRE:                  return gettext_noop("fire");
        case RING_ICE:                   return gettext_noop("ice");
#if TAG_MAJOR_VERSION == 34
        case RING_TELEPORTATION:         return gettext_noop("teleportation");
        case RING_TELEPORT_CONTROL:      return gettext_noop("teleport control");
#endif
        case AMU_MANA_REGENERATION: return gettext_noop("magic regeneration");
        case AMU_ACROBAT:           return gettext_noop("the acrobat");
#if TAG_MAJOR_VERSION == 34
        case AMU_RAGE:              return gettext_noop("rage");
        case AMU_THE_GOURMAND:      return gettext_noop("gourmand");
        case AMU_HARM:              return gettext_noop("harm");
        case AMU_CONSERVATION:      return gettext_noop("conservation");
        case AMU_CONTROLLED_FLIGHT: return gettext_noop("controlled flight");
        case AMU_INACCURACY:        return gettext_noop("inaccuracy");
#endif
        case AMU_NOTHING:           return gettext_noop("nothing");
        case AMU_GUARDIAN_SPIRIT:   return gettext_noop("guardian spirit");
        case AMU_FAITH:             return gettext_noop("faith");
        case AMU_REFLECTION:        return gettext_noop("reflection");
        case AMU_REGENERATION:      return gettext_noop("regeneration");
        default: return gettext_noop("buggy jewellery");
        }
    }
    else
    {
        if (jewellery_base_ability_string(jeweltype)[0] != '\0')
            return jewellery_base_ability_string(jeweltype);
        switch (static_cast<jewellery_type>(jeweltype))
        {
#if TAG_MAJOR_VERSION == 34
        case RING_REGENERATION:          return gettext_noop("obsoleteness");
        case RING_ATTENTION:             return gettext_noop("obsoleteness");
#endif
        case RING_PROTECTION:            return gettext_noop("AC");
        case RING_PROTECTION_FROM_FIRE:  return gettext_noop("rF+");
        case RING_POISON_RESISTANCE:     return gettext_noop("rPois");
        case RING_PROTECTION_FROM_COLD:  return gettext_noop("rC+");
        case RING_STRENGTH:              return gettext_noop("Str");
        case RING_SLAYING:               return gettext_noop("Slay");
        case RING_SEE_INVISIBLE:         return gettext_noop("sInv");
        case RING_RESIST_CORROSION:      return gettext_noop("rCorr");
        case RING_EVASION:               return gettext_noop("EV");
        case RING_STEALTH:               return gettext_noop("Stlth+");
        case RING_DEXTERITY:             return gettext_noop("Dex");
        case RING_INTELLIGENCE:          return gettext_noop("Int");
        case RING_MAGICAL_POWER:         return gettext_noop("MP+9");
        case RING_FLIGHT:                return gettext_noop("Fly");
        case RING_LIFE_PROTECTION:       return gettext_noop("rN+");
        case RING_WILLPOWER:             return gettext_noop("Will+");
        case AMU_REGENERATION:           return gettext_noop("Regen");
#if TAG_MAJOR_VERSION == 34
        case AMU_RAGE:                   return gettext_noop("+Rage");
#endif
        case AMU_ACROBAT:                return gettext_noop("Acrobat");
        case AMU_NOTHING:                return "";
        default: return gettext_noop("buggy");
        }
    }
}

/**
 * Get the name for the category of a type of jewellery.
 *
 * @param jeweltype     The jewellery_type of the item in question.
 * @return              A string describing the kind of jewellery it is.
 */
static const char* _jewellery_class_name(int jeweltype)
{
#if TAG_MAJOR_VERSION == 34
    if (jeweltype == RING_REGENERATION)
        return gettext_noop("ring of");
#endif

    if (jeweltype < RING_FIRST_RING || jeweltype >= NUM_JEWELLERY
        || jeweltype >= NUM_RINGS && jeweltype < AMU_FIRST_AMULET)
    {
        return gettext_noop("buggy"); // "buggy buggy jewellery"
    }

    if (jeweltype < NUM_RINGS)
        return gettext_noop("ring of");
    return gettext_noop("amulet of");
}

/**
 * Get the name for a type of jewellery.
 *
 * @param jeweltype     The jewellery_type of the item in question.
 * @return              The full name of the jewellery type in question.
 */
static string jewellery_type_name(int jeweltype)
{
    return make_stringf(gettext_noop("%s %s"), _jewellery_class_name(jeweltype),
                                 jewellery_effect_name(jeweltype));
}


static const char* ring_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        "", gettext_noop("encrusted "), gettext_noop("glowing "), gettext_noop("tubular "), gettext_noop("runed "), gettext_noop("blackened "),
        gettext_noop("scratched "), gettext_noop("small "), gettext_noop("large "), gettext_noop("twisted "), gettext_noop("shiny "), gettext_noop("notched "),
        gettext_noop("knobbly ")
    };
    COMPILE_CHECK(ARRAYSZ(secondary_strings) == NDSC_JEWEL_SEC);
    return secondary_strings[s % NDSC_JEWEL_SEC];
}

static const char* ring_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        gettext_noop("wooden"), gettext_noop("silver"), gettext_noop("golden"), gettext_noop("iron"), gettext_noop("steel"), gettext_noop("tourmaline"), gettext_noop("brass"),
        gettext_noop("copper"), gettext_noop("granite"), gettext_noop("ivory"), gettext_noop("ruby"), gettext_noop("marble"), gettext_noop("jade"), gettext_noop("glass"),
        gettext_noop("agate"), gettext_noop("bone"), gettext_noop("diamond"), gettext_noop("emerald"), gettext_noop("peridot"), gettext_noop("garnet"), gettext_noop("opal"),
        gettext_noop("pearl"), gettext_noop("coral"), gettext_noop("sapphire"), gettext_noop("cabochon"), gettext_noop("gilded"), gettext_noop("onyx"), gettext_noop("bronze"),
        gettext_noop("moonstone")
    };
    COMPILE_CHECK(ARRAYSZ(primary_strings) == NDSC_JEWEL_PRI);
    return primary_strings[p % NDSC_JEWEL_PRI];
}

static const char* amulet_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        gettext_noop("dented "), gettext_noop("square "), gettext_noop("thick "), gettext_noop("thin "), gettext_noop("runed "), gettext_noop("blackened "),
        gettext_noop("glowing "), gettext_noop("small "), gettext_noop("large "), gettext_noop("twisted "), gettext_noop("tiny "), gettext_noop("triangular "),
        gettext_noop("lumpy ")
    };
    COMPILE_CHECK(ARRAYSZ(secondary_strings) == NDSC_JEWEL_SEC);
    return secondary_strings[s % NDSC_JEWEL_SEC];
}

static const char* amulet_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        gettext_noop("sapphire"), gettext_noop("zirconium"), gettext_noop("golden"), gettext_noop("emerald"), gettext_noop("garnet"), gettext_noop("bronze"),
        gettext_noop("brass"), gettext_noop("copper"), gettext_noop("ruby"), gettext_noop("citrine"), gettext_noop("bone"), gettext_noop("platinum"), gettext_noop("jade"),
        gettext_noop("fluorescent"), gettext_noop("amethyst"), gettext_noop("cameo"), gettext_noop("pearl"), gettext_noop("blue"), gettext_noop("peridot"),
        gettext_noop("jasper"), gettext_noop("diamond"), gettext_noop("malachite"), gettext_noop("steel"), gettext_noop("cabochon"), gettext_noop("silver"),
        gettext_noop("soapstone"), gettext_noop("lapis lazuli"), gettext_noop("filigree"), gettext_noop("beryl")
    };
    COMPILE_CHECK(ARRAYSZ(primary_strings) == NDSC_JEWEL_PRI);
    return primary_strings[p % NDSC_JEWEL_PRI];
}

const char* rune_type_name(short p)
{
    switch (static_cast<rune_type>(p))
    {
    case RUNE_DIS:         return gettext_noop("iron");
    case RUNE_GEHENNA:     return gettext_noop("obsidian");
    case RUNE_COCYTUS:     return gettext_noop("icy");
    case RUNE_TARTARUS:    return gettext_noop("bone");
    case RUNE_SLIME:       return gettext_noop("slimy");
    case RUNE_VAULTS:      return gettext_noop("silver");
    case RUNE_SNAKE:       return gettext_noop("serpentine");
    case RUNE_ELF:         return gettext_noop("elven");
    case RUNE_TOMB:        return gettext_noop("golden");
    case RUNE_SWAMP:       return gettext_noop("decaying");
    case RUNE_SHOALS:      return gettext_noop("barnacled");
    case RUNE_SPIDER:      return gettext_noop("gossamer");
    case RUNE_FOREST:      return gettext_noop("mossy");

    // pandemonium and abyss runes:
    case RUNE_DEMONIC:     return gettext_noop("demonic");
    case RUNE_ABYSSAL:     return gettext_noop("abyssal");

    // special pandemonium runes:
    case RUNE_MNOLEG:      return gettext_noop("glowing");
    case RUNE_LOM_LOBON:   return gettext_noop("magical");
    case RUNE_CEREBOV:     return gettext_noop("fiery");
    case RUNE_GLOORX_VLOQ: return gettext_noop("dark");
    default:               return gettext_noop("buggy");
    }
}

static string misc_type_name(int type)
{
#if TAG_MAJOR_VERSION == 34
    if (is_deck_type(type))
        return gettext_noop("removed deck");
#endif

    switch (static_cast<misc_item_type>(type))
    {
#if TAG_MAJOR_VERSION == 34
    case MISC_CRYSTAL_BALL_OF_ENERGY:    return gettext_noop("removed crystal ball");
#endif
    case MISC_BOX_OF_BEASTS:             return gettext_noop("box of beasts");
#if TAG_MAJOR_VERSION == 34
    case MISC_BUGGY_EBONY_CASKET:        return gettext_noop("removed ebony casket");
    case MISC_FAN_OF_GALES:              return gettext_noop("removed fan of gales");
    case MISC_LAMP_OF_FIRE:              return gettext_noop("removed lamp of fire");
    case MISC_BUGGY_LANTERN_OF_SHADOWS:  return gettext_noop("removed lantern of shadows");
#endif
    case MISC_HORN_OF_GERYON:            return gettext_noop("horn of Geryon");
    case MISC_LIGHTNING_ROD:             return gettext_noop("lightning rod");
#if TAG_MAJOR_VERSION == 34
    case MISC_BOTTLED_EFREET:            return gettext_noop("empty flask");
    case MISC_RUNE_OF_ZOT:               return gettext_noop("obsolete rune of zot");
    case MISC_STONE_OF_TREMORS:          return gettext_noop("removed stone of tremors");
#endif
    case MISC_QUAD_DAMAGE:               return gettext_noop("quad damage");
    case MISC_PHIAL_OF_FLOODS:           return gettext_noop("phial of floods");
#if TAG_MAJOR_VERSION == 34
    case MISC_SACK_OF_SPIDERS:           return gettext_noop("removed sack of spiders");
#endif
    case MISC_PHANTOM_MIRROR:            return gettext_noop("phantom mirror");
    case MISC_ZIGGURAT:                  return gettext_noop("figurine of a ziggurat");
    case MISC_XOMS_CHESSBOARD:           return gettext_noop("piece from Xom's chessboard");
    case MISC_TIN_OF_TREMORSTONES:       return gettext_noop("tin of tremorstones");
    case MISC_CONDENSER_VANE:            return gettext_noop("condenser vane");

    default:
        return gettext_noop("buggy miscellaneous item");
    }
}

static bool _book_visually_special(uint32_t s)
{
    return s & 128; // one in ten books; c.f. item_colour()
}

static const char* book_secondary_string(uint32_t s)
{
    if (!_book_visually_special(s))
        return "";

    static const char* const secondary_strings[] = {
        "", gettext_noop("chunky "), gettext_noop("thick "), gettext_noop("thin "), gettext_noop("wide "), gettext_noop("glowing "),
        gettext_noop("dog-eared "), gettext_noop("oblong "), gettext_noop("runed "), "", "", ""
    };
    return secondary_strings[(s / NDSC_BOOK_PRI) % ARRAYSZ(secondary_strings)];
}

static const char* book_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        gettext_noop("paperback"), gettext_noop("hardcover"), gettext_noop("leatherbound"), gettext_noop("metal-bound"), gettext_noop("papyrus"),
    };
    COMPILE_CHECK(NDSC_BOOK_PRI == ARRAYSZ(primary_strings));

    return primary_strings[p % ARRAYSZ(primary_strings)];
}

static const char* _book_type_name(int booktype)
{
    switch (static_cast<book_type>(booktype))
    {
    case BOOK_MINOR_MAGIC:            return gettext_noop("Minor Magic");
    case BOOK_CONJURATIONS:           return gettext_noop("Conjurations");
    case BOOK_FLAMES:                 return gettext_noop("Flames");
    case BOOK_FROST:                  return gettext_noop("Frost");
    case BOOK_SUMMONINGS:             return gettext_noop("Summonings");
    case BOOK_FIRE:                   return gettext_noop("Fire");
    case BOOK_ICE:                    return gettext_noop("Ice");
    case BOOK_SPATIAL_TRANSLOCATIONS: return gettext_noop("Spatial Translocations");
    case BOOK_HEXES:                  return gettext_noop("Hexes");
    case BOOK_TEMPESTS:               return gettext_noop("the Tempests");
    case BOOK_DEATH:                  return gettext_noop("Death");
    case BOOK_MISFORTUNE:             return gettext_noop("Misfortune");
    case BOOK_CHANGES:                return gettext_noop("Changes");
    case BOOK_TRANSFIGURATIONS:       return gettext_noop("Transfigurations");
#if TAG_MAJOR_VERSION == 34
    case BOOK_BATTLE:                 return gettext_noop("Battle");
#endif
    case BOOK_CLOUDS:                 return gettext_noop("Clouds");
    case BOOK_NECROMANCY:             return gettext_noop("Necromancy");
    case BOOK_CALLINGS:               return gettext_noop("Callings");
    case BOOK_MALEDICT:               return gettext_noop("Maledictions");
    case BOOK_AIR:                    return gettext_noop("Air");
    case BOOK_SKY:                    return gettext_noop("the Sky");
    case BOOK_WARP:                   return gettext_noop("the Warp");
#if TAG_MAJOR_VERSION == 34
    case BOOK_ENVENOMATIONS:          return gettext_noop("Envenomations");
#endif
    case BOOK_ANNIHILATIONS:          return gettext_noop("Annihilations");
    case BOOK_UNLIFE:                 return gettext_noop("Unlife");
#if TAG_MAJOR_VERSION == 34
    case BOOK_CONTROL:                return gettext_noop("Control");
#endif
    case BOOK_GEOMANCY:               return gettext_noop("Geomancy");
    case BOOK_EARTH:                  return gettext_noop("the Earth");
#if TAG_MAJOR_VERSION == 34
    case BOOK_WIZARDRY:               return gettext_noop("Wizardry");
#endif
    case BOOK_POWER:                  return gettext_noop("Power");
    case BOOK_CANTRIPS:               return gettext_noop("Cantrips");
    case BOOK_PARTY_TRICKS:           return gettext_noop("Party Tricks");
    case BOOK_DEBILITATION:           return gettext_noop("Debilitation");
    case BOOK_DRAGON:                 return gettext_noop("the Dragon");
    case BOOK_BURGLARY:               return gettext_noop("Burglary");
    case BOOK_DREAMS:                 return gettext_noop("Dreams");
    case BOOK_ALCHEMY:                return gettext_noop("Alchemy");
#if TAG_MAJOR_VERSION == 34
    case BOOK_BEASTS:                 return gettext_noop("Beasts");
#endif
    case BOOK_RANDART_LEVEL:          return gettext_noop("Fixed Level");
    case BOOK_RANDART_THEME:          return gettext_noop("Fixed Theme");
    default:                          return gettext_noop("Bugginess");
    }
}

static const char* staff_secondary_string(uint32_t s)
{
    static const char* const secondary_strings[] = {
        gettext_noop("crooked "), gettext_noop("knobbly "), gettext_noop("weird "), gettext_noop("gnarled "), gettext_noop("thin "), gettext_noop("curved "),
        gettext_noop("twisted "), gettext_noop("thick "), gettext_noop("long "), gettext_noop("short "),
    };
    COMPILE_CHECK(NDSC_STAVE_SEC == ARRAYSZ(secondary_strings));
    return secondary_strings[s % ARRAYSZ(secondary_strings)];
}

static const char* staff_primary_string(uint32_t p)
{
    static const char* const primary_strings[] = {
        gettext_noop("glowing "), gettext_noop("jewelled "), gettext_noop("runed "), gettext_noop("smoking ")
    };
    COMPILE_CHECK(NDSC_STAVE_PRI == ARRAYSZ(primary_strings));
    return primary_strings[p % ARRAYSZ(primary_strings)];
}

static const char* staff_type_name(int stafftype)
{
    switch ((stave_type)stafftype)
    {
    case STAFF_FIRE:        return gettext_noop("fire");
    case STAFF_COLD:        return gettext_noop("cold");
    case STAFF_POISON:      return gettext_noop("poison");
    case STAFF_DEATH:       return gettext_noop("death");
    case STAFF_CONJURATION: return gettext_noop("conjuration");
    case STAFF_AIR:         return gettext_noop("air");
    case STAFF_EARTH:       return gettext_noop("earth");
    default:                return item_type_removed(OBJ_STAVES, stafftype)
                                ? gettext_noop("removedness")
                                : gettext_noop("bugginess");
    }
}

const char *base_type_string(const item_def &item)
{
    return base_type_string(item.base_type);
}

const char *base_type_string(object_class_type type)
{
    switch (type)
    {
    case OBJ_WEAPONS: return gettext_noop("weapon");
    case OBJ_MISSILES: return gettext_noop("missile");
    case OBJ_ARMOUR: return gettext_noop("armour");
    case OBJ_WANDS: return gettext_noop("wand");
    case OBJ_SCROLLS: return gettext_noop("scroll");
    case OBJ_JEWELLERY: return gettext_noop("jewellery");
    case OBJ_POTIONS: return gettext_noop("potion");
    case OBJ_BOOKS: return gettext_noop("book");
    case OBJ_STAVES: return gettext_noop("staff");
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS: return gettext_noop("removed rod");
#endif
    case OBJ_ORBS: return gettext_noop("orb");
    case OBJ_MISCELLANY: return gettext_noop("miscellaneous");
    case OBJ_CORPSES: return gettext_noop("corpse");
    case OBJ_GOLD: return gettext_noop("gold");
    case OBJ_RUNES: return gettext_noop("rune");
    default: return "";
    }
}

string sub_type_string(const item_def &item, bool known)
{
    const object_class_type type = item.base_type;
    const int sub_type = item.sub_type;

    switch (type)
    {
    case OBJ_WEAPONS:  // deliberate fall through, as XXX_prop is a local
    case OBJ_MISSILES: // variable to item-prop.cc.
    case OBJ_ARMOUR:
        return item_base_name(type, sub_type);
    case OBJ_WANDS: return _wand_type_name(sub_type);
    case OBJ_SCROLLS: return scroll_type_name(sub_type);
    case OBJ_JEWELLERY: return jewellery_type_name(sub_type);
    case OBJ_POTIONS: return potion_type_name(sub_type);
    case OBJ_BOOKS:
    {
        if (sub_type == BOOK_MANUAL)
        {
            if (!known)
                return gettext_noop("manual");
            string bookname = gettext_noop("manual of ");
            bookname += skill_name(static_cast<skill_type>(item.plus));
            return bookname;
        }
        else if (sub_type == BOOK_NECRONOMICON)
            return gettext_noop("Necronomicon");
        else if (sub_type == BOOK_GRAND_GRIMOIRE)
            return gettext_noop("Grand Grimoire");
#if TAG_MAJOR_VERSION == 34
        else if (sub_type == BOOK_BUGGY_DESTRUCTION)
            return gettext_noop("tome of obsoleteness");
#endif
        else if (sub_type == BOOK_YOUNG_POISONERS)
            return gettext_noop("Young Poisoner's Handbook");
        else if (sub_type == BOOK_FEN)
            return gettext_noop("Fen Folio");
#if TAG_MAJOR_VERSION == 34
        else if (sub_type == BOOK_AKASHIC_RECORD)
            return gettext_noop("Akashic Record");
#endif

        return string(gettext_noop("book of ")) + _book_type_name(sub_type);
    }
    case OBJ_STAVES: return staff_type_name(static_cast<stave_type>(sub_type));
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:   return gettext_noop("removed rod");
#endif
    case OBJ_MISCELLANY: return misc_type_name(sub_type);
    // these repeat as base_type_string
    case OBJ_ORBS: return gettext_noop("orb of Zot");
    case OBJ_CORPSES: return gettext_noop("corpse");
    case OBJ_GOLD: return gettext_noop("gold");
    case OBJ_RUNES: return gettext_noop("rune of Zot");
    default: return "";
    }
}

/**
 * What's the name for the weapon used by a given ghost / pan lord?
 *
 * There's no actual weapon info, just brand, so we have to improvise...
 *
 * @param brand     The brand_type used by the ghost or pan lord.
 * @param mtype     Monster type; determines whether the fake weapon is
 *                  described as a `weapon` or a `touch`.
 * @return          The name of the ghost's weapon (e.g. "weapon of flaming",
 *                  "antimagic weapon"). SPWPN_NORMAL returns "".
 */
string ghost_brand_name(brand_type brand, monster_type mtype)
{
    if (brand == SPWPN_NORMAL)
        return "";
    const bool weapon = mtype != MONS_PANDEMONIUM_LORD;
    if (weapon)
    {
        // n.b. vorpal only works if it is adjectival
        if (brand_prefers_adj.count(brand))
            return make_stringf(gettext_noop("%s weapon"), brand_type_adj(brand));
        else
            return make_stringf(gettext_noop("weapon of %s"), brand_type_name(brand, false));
    }
    else
        return make_stringf(gettext_noop("%s touch"), brand_type_adj(brand));
}

string ego_type_string(const item_def &item, bool terse)
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return armour_ego_name(item, terse);
    case OBJ_WEAPONS:
        if (get_weapon_brand(item) != SPWPN_NORMAL)
            return weapon_brand_name(item, terse);
        else
            return "";
    case OBJ_MISSILES:
        // HACKHACKHACK
        if (item.props.exists(DAMNATION_BOLT_KEY))
            return gettext_noop("damnation");
        return missile_brand_name(item, terse ? MBN_TERSE : MBN_BRAND);
    case OBJ_JEWELLERY:
        return jewellery_effect_name(item.sub_type, terse);
    default:
        return "";
    }
}

/**
 * When naming the given item, should the base name be used?
 */
static bool _use_basename(const item_def &item, description_level_type desc,
                          bool ident)
{
    const bool know_type = ident || item_type_known(item);
    return desc == DESC_BASENAME
           || desc == DESC_DBNAME && !know_type;
}

/**
 * When naming the given item, should identifiable properties be mentioned?
 */
static bool _know_any_ident(const item_def &item, description_level_type desc,
                            bool ident)
{
    return desc != DESC_QUALNAME && desc != DESC_DBNAME
           && !_use_basename(item, desc, ident);
}

/**
 * When naming the given item, should the specified identifiable property be
 * mentioned?
 */
static bool _know_ident(const item_def &item, description_level_type desc,
                        bool ident, iflags_t ignore_flags,
                        item_status_flag_type vprop)
{
    return _know_any_ident(item, desc, ident)
            && !testbits(ignore_flags, vprop)
            && (ident || item_ident(item, vprop));
}

/**
 * When naming the given item, should the pluses be mentioned?
 */
static bool _know_pluses(const item_def &item, description_level_type desc,
                          bool ident, iflags_t ignore_flags)
{
    return _know_ident(item, desc, ident, ignore_flags, ISFLAG_KNOW_PLUSES);
}

/**
 * When naming the given item, should the brand be mentioned?
 */
static bool _know_ego(const item_def &item, description_level_type desc,
                         bool ident, iflags_t ignore_flags)
{
    return _know_any_ident(item, desc, ident)
           && !testbits(ignore_flags, ISFLAG_KNOW_TYPE)
           && (ident || item_type_known(item));
}

/**
 * The plus-describing prefix to a weapon's name, including trailing space.
 */
static string _plus_prefix(const item_def &weap)
{
    if (is_unrandom_artefact(weap, UNRAND_WOE))
        return "+∞ ";
    return make_stringf("%+d ", weap.plus);
}

/**
 * Cosmetic text for weapons (e.g. glowing, runed). Includes trailing space,
 * if appropriate. (Empty if there is no cosmetic property, or if it's
 * marked to be ignored.)
 */
static string _cosmetic_text(const item_def &weap, iflags_t ignore_flags)
{
    const iflags_t desc = get_equip_desc(weap);
    if (testbits(ignore_flags, desc))
        return "";

    switch (desc)
    {
        case ISFLAG_RUNED:
            return gettext_noop("runed ");
        case ISFLAG_GLOWING:
            return gettext_noop("glowing ");
        default:
            return "";
    }
}

/**
 * Surrounds a given string with the weapon's brand-describing prefix/suffix
 * as appropriate.
 */
string weapon_brand_desc(const char *body, const item_def &weap,
                         bool terse, brand_type override_brand)
{

    const string brand_name = weapon_brand_name(weap, terse, override_brand);

    if (brand_name.empty())
        return body;

    if (terse)
        return make_stringf(gettext_noop("%s (%s)"), body, brand_name.c_str());

    const brand_type brand = override_brand ? override_brand :
                             get_weapon_brand(weap);

    if (brand_prefers_adj.count(brand))
        return make_stringf(gettext_noop("%s %s"), brand_type_adj(brand), body);
    else if (brand == SPWPN_NORMAL)
    {
        if (get_equip_desc(weap))
            return make_stringf(gettext_noop("enchanted %s"), body);
        else
            return body;
    }
    else
        return make_stringf(gettext_noop("%s of %s"), body, brand_name.c_str());
}

/**
 * Build the appropriate name for a given weapon.
 *
 * @param weap          The weapon in question.
 * @param desc          The type of name to provide. (E.g. the name to be used
 *                      in database lookups for description, or...)
 * @param terse         Whether to provide a terse version of the name for
 *                      display in the HUD.
 * @param ident         Whether the weapon should be named as if it were
 *                      identified.
 * @param inscr         Whether an inscription will be added later.
 * @param ignore_flags  Identification flags on the weapon to ignore.
 *
 * @return              A name for the weapon.
 *                      TODO: example
 */
static string _name_weapon(const item_def &weap, description_level_type desc,
                           bool terse, bool ident, bool inscr,
                           iflags_t ignore_flags)
{
    const bool dbname   = (desc == DESC_DBNAME);
    const bool basename = _use_basename(weap, desc, ident);
    const bool qualname = (desc == DESC_QUALNAME);

    const bool know_pluses = _know_pluses(weap, desc, ident, ignore_flags);
    const bool know_ego =    _know_ego(weap, desc, ident, ignore_flags);

    const string curse_prefix = !dbname && !terse && weap.cursed() ? gettext_noop("cursed ") : "";
    const string plus_text = know_pluses ? _plus_prefix(weap) : "";

    if (is_artefact(weap) && !dbname)
    {
        const string long_name = curse_prefix + plus_text
                                 + get_artefact_name(weap, ident);

        // crop long artefact names when not controlled by webtiles -
        // webtiles displays weapon names across multiple lines
#ifdef USE_TILE_WEB
        if (!tiles.is_controlled_from_web())
#endif
        {
            const bool has_inscript = desc != DESC_BASENAME
                                   && desc != DESC_DBNAME
                                   && inscr;
            const string inscription = _item_inscription(weap);

            const int total_length = long_name.size()
                                     + (has_inscript ? inscription.size() : 0);
            const string inv_slot_text = "x) ";
            const int max_length = crawl_view.hudsz.x - inv_slot_text.size();
            if (!terse || total_length <= max_length)
                return long_name;
        }
#ifdef USE_TILE_WEB
        else
            return long_name;
#endif

        // special case: these two shouldn't ever have their base name revealed
        // (since showing 'eudaemon blade' is unhelpful in the former case, and
        // showing 'broad axe' is misleading in the latter)
        // could be a flag, but doesn't seem worthwhile for only two items
        if (is_unrandom_artefact(weap, UNRAND_ZEALOT_SWORD)
            || is_unrandom_artefact(weap, UNRAND_DEMON_AXE))
        {
            return long_name;
        }

        const string short_name
            = curse_prefix + plus_text + get_artefact_base_name(weap, true);
        return short_name;
    }

    const bool show_cosmetic = !basename && !qualname && !dbname
                               && !know_pluses && !know_ego
                               && !terse
                               && !(ignore_flags & ISFLAG_COSMETIC_MASK);

    const string cosmetic_text
        = show_cosmetic ? _cosmetic_text(weap, ignore_flags) : "";
    const string base_name = item_base_name(weap);
    const string name_with_ego
        = know_ego ? weapon_brand_desc(base_name.c_str(), weap, terse)
        : base_name;
    const string curse_suffix
        = weap.cursed() && terse && !dbname && !qualname ? gettext_noop(" (curse)") :  "";
    return curse_prefix + plus_text + cosmetic_text
           + name_with_ego + curse_suffix;
}

// Note that "terse" is only currently used for the "in hand" listing on
// the game screen.
string item_def::name_aux(description_level_type desc, bool terse, bool ident,
                          bool with_inscription, iflags_t ignore_flags) const
{
    // Shortcuts
    const int item_typ   = sub_type;

    const bool know_type = ident || item_type_known(*this);

    const bool dbname   = (desc == DESC_DBNAME);
    const bool basename = _use_basename(*this, desc, ident);
    const bool qualname = (desc == DESC_QUALNAME);

    const bool know_pluses = _know_pluses(*this, desc, ident, ignore_flags);
    const bool know_brand =  _know_ego(*this, desc, ident, ignore_flags);

    const bool know_ego = know_brand;

    // Display runed/glowing/embroidered etc?
    // Only display this if brand is unknown.
    const bool show_cosmetic = !know_pluses && !know_brand
                               && !basename && !qualname && !dbname
                               && !terse
                               && !(ignore_flags & ISFLAG_COSMETIC_MASK);

    const bool need_plural = !basename && !dbname;

    ostringstream buff;

    switch (base_type)
    {
    case OBJ_WEAPONS:
        buff << _name_weapon(*this, desc, terse, ident, with_inscription,
                             ignore_flags);
        break;

    case OBJ_MISSILES:
    {
        special_missile_type msl_brand = get_ammo_brand(*this);

        if (!terse && !dbname && !basename)
        {
            if (props.exists(DAMNATION_BOLT_KEY)) // hack alert
                buff << gettext_noop("damnation ");
            else if (_missile_brand_is_prefix(msl_brand)) // see below for postfix brands
                buff << missile_brand_name(*this, MBN_NAME) << ' ';
        }

        buff << ammo_name(static_cast<missile_type>(item_typ));

        if (msl_brand != SPMSL_NORMAL
            && !basename && !dbname)
        {
            if (terse)
            {
                if (props.exists(DAMNATION_BOLT_KEY)) // still a hack
                    buff << gettext_noop(" (damnation)");
                else
                    buff << " (" <<  missile_brand_name(*this, MBN_TERSE) << ")";
            }
            else if (_missile_brand_is_postfix(msl_brand)) // see above for prefix brands
                buff << " of " << missile_brand_name(*this, MBN_NAME);
        }

        break;
    }
    case OBJ_ARMOUR:
        if (!terse && cursed())
            buff << gettext_noop("cursed ");

        // If we know enough to know it has *something* ('shiny' etc),
        // but we know it has no ego, it must have a plus. (or maybe a curse.)
        // If we don't know what the plus is, call it 'enchanted'.
        if (!terse && know_ego && get_armour_ego_type(*this) == SPARM_NORMAL &&
            !know_pluses && !is_artefact(*this) && get_equip_desc(*this))
        {
            buff << gettext_noop("enchanted ");
        }

        // Don't list unenchantable armor as +0.
        if (know_pluses && armour_is_enchantable(*this))
            buff << make_stringf("%+d ", plus);

        if (item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
            buff << gettext_noop("pair of ");

        if (is_artefact(*this) && !dbname)
        {
            buff << get_artefact_name(*this, ident);
            break;
        }

        if (show_cosmetic)
        {
            switch (get_equip_desc(*this))
            {
            case ISFLAG_EMBROIDERED_SHINY:
                if (testbits(ignore_flags, ISFLAG_EMBROIDERED_SHINY))
                    break;
                if (item_typ == ARM_ROBE || item_typ == ARM_CLOAK
                    || item_typ == ARM_GLOVES || item_typ == ARM_BOOTS
                    || item_typ == ARM_SCARF
                    || get_armour_slot(*this) == EQ_HELMET
                       && !is_hard_helmet(*this))
                {
                    buff << gettext_noop("embroidered ");
                }
                else if (item_typ != ARM_LEATHER_ARMOUR
                         && item_typ != ARM_ANIMAL_SKIN)
                {
                    buff << gettext_noop("shiny ");
                }
                else
                    buff << gettext_noop("dyed ");
                break;

            case ISFLAG_RUNED:
                if (!testbits(ignore_flags, ISFLAG_RUNED))
                    buff << gettext_noop("runed ");
                break;

            case ISFLAG_GLOWING:
                if (!testbits(ignore_flags, ISFLAG_GLOWING))
                    buff << gettext_noop("glowing ");
                break;
            }
        }

        buff << item_base_name(*this);

        if (know_ego && !is_artefact(*this))
        {
            const special_armour_type sparm = get_armour_ego_type(*this);

            if (sparm != SPARM_NORMAL)
            {
                if (!terse)
                    buff << " of ";
                else
                    buff << " {";
                buff << armour_ego_name(*this, terse);
                if (terse)
                    buff << "}";
            }
        }

        if (cursed() && terse && !dbname && !qualname)
            buff << gettext_noop(" (curse)");
        break;

    case OBJ_WANDS:
        if (basename)
        {
            buff << gettext_noop("wand");
            break;
        }

        if (know_type)
            buff << gettext_noop("wand of ") << _wand_type_name(item_typ);
        else
        {
            buff << wand_secondary_string(subtype_rnd / NDSC_WAND_PRI)
                 << wand_primary_string(subtype_rnd % NDSC_WAND_PRI)
                 << gettext_noop(" wand");
        }

        if (dbname)
            break;

        if (know_type && charges > 0)
            buff << " (" << charges << ")";

        break;

    case OBJ_POTIONS:
        if (basename)
        {
            buff << gettext_noop("potion");
            break;
        }

        if (know_type)
            buff << gettext_noop("potion of ") << potion_type_name(item_typ);
        else
        {
            const int pqual   = PQUAL(subtype_rnd);
            const int pcolour = PCOLOUR(subtype_rnd);

            static const char *potion_qualifiers[] =
            {
                "",  gettext_noop("bubbling "), gettext_noop("fuming "), gettext_noop("fizzy "), gettext_noop("viscous "), gettext_noop("lumpy "),
                gettext_noop("smoky "), gettext_noop("glowing "), gettext_noop("sedimented "), gettext_noop("metallic "), gettext_noop("murky "),
                gettext_noop("gluggy "), gettext_noop("oily "), gettext_noop("slimy "), gettext_noop("emulsified "),
            };
            COMPILE_CHECK(ARRAYSZ(potion_qualifiers) == PDQ_NQUALS);

            static const char *potion_colours[] =
            {
#if TAG_MAJOR_VERSION == 34
                gettext_noop("clear"),
#endif
                gettext_noop("blue"), gettext_noop("black"), gettext_noop("silvery"), gettext_noop("cyan"), gettext_noop("purple"), gettext_noop("orange"),
                gettext_noop("inky"), gettext_noop("red"), gettext_noop("yellow"), gettext_noop("green"), gettext_noop("brown"), gettext_noop("ruby"), gettext_noop("white"),
                gettext_noop("emerald"), gettext_noop("grey"), gettext_noop("pink"), gettext_noop("coppery"), gettext_noop("golden"), gettext_noop("dark"), gettext_noop("puce"),
                gettext_noop("amethyst"), gettext_noop("sapphire"),
            };
            COMPILE_CHECK(ARRAYSZ(potion_colours) == PDC_NCOLOURS);

            const char *qualifier =
                (pqual < 0 || pqual >= PDQ_NQUALS) ? gettext_noop("bug-filled ")
                                    : potion_qualifiers[pqual];

            const char *clr =  (pcolour < 0 || pcolour >= PDC_NCOLOURS) ?
                                   gettext_noop("bogus") : potion_colours[pcolour];

            buff << qualifier << clr << gettext_noop(" potion");
        }
        break;

#if TAG_MAJOR_VERSION == 34
    case OBJ_FOOD:
        buff << gettext_noop("removed food"); break;
        break;
#endif

    case OBJ_SCROLLS:
        buff << gettext_noop("scroll");
        if (basename)
            break;
        else
            buff << " ";

        if (know_type)
            buff << "of " << scroll_type_name(item_typ);
        else
            buff << gettext_noop("labelled ") << make_name(subtype_rnd, MNAME_SCROLL);
        break;

    case OBJ_JEWELLERY:
    {
        if (basename)
        {
            if (jewellery_is_amulet(*this))
                buff << gettext_noop("amulet");
            else
                buff << gettext_noop("ring");

            break;
        }

        const bool is_randart = is_artefact(*this);

        if (!terse && cursed())
            buff << gettext_noop("cursed ");

        if (is_randart && !dbname)
        {
            buff << get_artefact_name(*this, ident);
            break;
        }

        if (know_type)
        {
            if (!dbname && jewellery_has_pluses(*this))
                buff << make_stringf("%+d ", plus);

            buff << jewellery_type_name(item_typ);
        }
        else
        {
            if (jewellery_is_amulet(*this))
            {
                buff << amulet_secondary_string(subtype_rnd / NDSC_JEWEL_PRI)
                     << amulet_primary_string(subtype_rnd % NDSC_JEWEL_PRI)
                     << gettext_noop(" amulet");
            }
            else  // i.e., a ring
            {
                buff << ring_secondary_string(subtype_rnd / NDSC_JEWEL_PRI)
                     << ring_primary_string(subtype_rnd % NDSC_JEWEL_PRI)
                     << gettext_noop(" ring");
            }
        }
        if (cursed() && terse && !dbname && !qualname)
            buff << gettext_noop(" (curse)");
        break;
    }
    case OBJ_MISCELLANY:
    {
        if (!dbname && item_typ == MISC_ZIGGURAT && you.zigs_completed > 0)
            buff << "+" << you.zigs_completed << " ";

        buff << misc_type_name(item_typ);

        if (is_xp_evoker(*this) && !dbname && !evoker_charges(sub_type))
            buff << gettext_noop(" (inert)");
        else if (is_xp_evoker(*this) &&
                 !dbname && evoker_max_charges(sub_type) > 1)
        {
            buff << " (" << evoker_charges(sub_type) << "/"
                 << evoker_max_charges(sub_type) << ")";
        }

        break;
    }

    case OBJ_BOOKS:
        if (is_random_artefact(*this) && !dbname && !basename)
        {
            buff << get_artefact_name(*this, ident);
            if (!know_type)
                buff << gettext_noop("book");
            break;
        }
        if (basename)
            buff << (item_typ == BOOK_MANUAL ? gettext_noop("manual") : gettext_noop("book"));
        else if (!know_type)
        {
            buff << book_secondary_string(rnd)
                 << book_primary_string(rnd) << " "
                 << (item_typ == BOOK_MANUAL ? gettext_noop("manual") : gettext_noop("book"));
        }
        else
            buff << sub_type_string(*this, !dbname);
        break;

#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
        buff << gettext_noop("removed rod");
        break;
#endif

    case OBJ_STAVES:
        if (!terse && cursed())
            buff << gettext_noop("cursed ");

        if (!know_type)
        {
            if (!basename)
            {
                buff << staff_secondary_string(subtype_rnd / NDSC_STAVE_PRI)
                     << staff_primary_string(subtype_rnd % NDSC_STAVE_PRI);
            }

            buff << gettext_noop("staff");
        }
        else
            buff << gettext_noop("staff of ") << staff_type_name(item_typ);

        if (cursed() && terse && !dbname && !qualname)
            buff << gettext_noop(" (curse)");
        break;

    // rearranged 15 Apr 2000 {dlb}:
    case OBJ_ORBS:
        buff.str(gettext_noop("Orb of Zot"));
        break;

    case OBJ_RUNES:
        if (!dbname)
            buff << rune_type_name(sub_type) << " ";
        buff << gettext_noop("rune of Zot");
        break;

    case OBJ_GOLD:
        buff << gettext_noop("gold piece");
        break;

    case OBJ_CORPSES:
    {
        if (dbname && item_typ == CORPSE_SKELETON)
            return gettext_noop("decaying skeleton");

        monster_flags_t name_flags;
        const string _name = get_corpse_name(*this, &name_flags);
        const monster_flags_t name_type = name_flags & MF_NAME_MASK;

        const bool shaped = starts_with(_name, gettext_noop("shaped "));

        if (!_name.empty() && name_type == MF_NAME_ADJECTIVE)
            buff << _name << " ";

        if ((name_flags & MF_NAME_SPECIES) && name_type == MF_NAME_REPLACE)
            buff << _name << " ";
        else if (!dbname && !starts_with(_name, "the "))
        {
            const monster_type mc = mon_type;
            if (!(mons_is_unique(mc) && mons_species(mc) == mc))
                buff << mons_type_name(mc, DESC_PLAIN) << ' ';

            if (!_name.empty() && shaped)
                buff << _name << ' ';
        }

        if (item_typ == CORPSE_BODY)
            buff << gettext_noop("corpse");
        else if (item_typ == CORPSE_SKELETON)
            buff << gettext_noop("skeleton");
        else
            buff << gettext_noop("corpse bug");

        if (!_name.empty() && !shaped && name_type != MF_NAME_ADJECTIVE
            && !(name_flags & MF_NAME_SPECIES) && name_type != MF_NAME_SUFFIX
            && !dbname)
        {
            buff << " of " << _name;
        }
        break;
    }

    default:
        buff << "!";
    }

    // One plural to rule them all.
    if (need_plural && quantity > 1 && !basename && !qualname)
        buff.str(pluralise(buff.str()));

    // debugging output -- oops, I probably block it above ... dang! {dlb}
    if (buff.str().length() < 3)
    {
        buff << "bad item (cl:" << static_cast<int>(base_type)
             << ",ty:" << item_typ << ",pl:" << plus
             << ",pl2:" << plus2 << ",sp:" << special
             << ",qu:" << quantity << ")";
    }

    return buff.str();
}

// WARNING: You can break save compatibility if you edit this without
// amending tags.cc to properly marshall the change.
bool item_type_has_ids(object_class_type base_type)
{
    COMPILE_CHECK(NUM_WEAPONS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_MISSILES   < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_ARMOURS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_WANDS      < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_SCROLLS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_JEWELLERY  < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_POTIONS    < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_BOOKS      < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_STAVES     < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_MISCELLANY < MAX_SUBTYPES);
#if TAG_MAJOR_VERSION == 34
    COMPILE_CHECK(NUM_RODS       < MAX_SUBTYPES);
    COMPILE_CHECK(NUM_FOODS      < MAX_SUBTYPES);
#endif

    return base_type == OBJ_WANDS || base_type == OBJ_SCROLLS
        || base_type == OBJ_JEWELLERY || base_type == OBJ_POTIONS
        || base_type == OBJ_STAVES || base_type == OBJ_BOOKS;
}

bool item_brand_known(const item_def& item)
{
    return item_ident(item, ISFLAG_KNOW_TYPE)
           || is_artefact(item)
           && artefact_known_property(item, ARTP_BRAND);
}

bool item_type_known(const item_def& item)
{
    if (item_ident(item, ISFLAG_KNOW_TYPE))
        return true;

    // Artefacts have different descriptions from other items,
    // so we can't use general item knowledge for them.
    if (is_artefact(item))
        return false;

    if (item.base_type == OBJ_MISSILES)
        return true;

    if (item.base_type == OBJ_MISCELLANY)
        return true;

#if TAG_MAJOR_VERSION == 34
    if (item.is_type(OBJ_BOOKS, BOOK_BUGGY_DESTRUCTION))
        return true;
#endif

    if (item.is_type(OBJ_BOOKS, BOOK_MANUAL))
        return false;

    if (!item_type_has_ids(item.base_type))
        return false;
    return you.type_ids[item.base_type][item.sub_type];
}

bool item_type_unknown(const item_def& item)
{
    if (item_type_known(item))
        return false;

    if (is_artefact(item))
        return true;

    return item_type_has_ids(item.base_type);
}

bool item_type_known(const object_class_type base_type, const int sub_type)
{
    if (!item_type_has_ids(base_type))
        return false;
    return you.type_ids[base_type][sub_type];
}

bool set_ident_type(item_def &item, bool identify)
{
    if (is_artefact(item) || crawl_state.game_is_arena())
        return false;

    if (!set_ident_type(item.base_type, item.sub_type, identify))
        return false;

    if (in_inventory(item))
    {
        shopping_list.cull_identical_items(item);
        if (identify)
            item_skills(item, you.skills_to_show);
    }

    if (identify && notes_are_active()
        && is_interesting_item(item)
        && !(item.flags & (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET)))
    {
        // Make a note of it.
        take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_A),
                       origin_desc(item)));

        // Sometimes (e.g. shops) you can ID an item before you get it;
        // don't note twice in those cases.
        item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
    }

    return true;
}

bool set_ident_type(object_class_type basetype, int subtype, bool identify)
{
    if (!item_type_has_ids(basetype))
        return false;

    if (you.type_ids[basetype][subtype] == identify)
        return false;

    you.type_ids[basetype][subtype] = identify;
    request_autoinscribe();

    // Our item knowledge changed in a way that could possibly affect shop
    // prices.
    shopping_list.item_type_identified(basetype, subtype);

    // We identified something, maybe we identified other things by process of
    // elimination.
    if (identify && !(you.pending_revival || crawl_state.updating_scores))
        _maybe_identify_pack_item();

    return true;
}

void pack_item_identify_message(int base_type, int sub_type)
{
    for (const auto &item : you.inv)
        if (item.defined() && item.is_type(base_type, sub_type))
            mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}

bool get_ident_type(const item_def &item)
{
    if (is_artefact(item))
        return false;

    return get_ident_type(item.base_type, item.sub_type);
}

bool get_ident_type(object_class_type basetype, int subtype)
{
    if (!item_type_has_ids(basetype))
        return false;
    ASSERT(subtype < MAX_SUBTYPES);
    return you.type_ids[basetype][subtype];
}

static MenuEntry* _fixup_runeorb_entry(MenuEntry* me)
{
    auto entry = static_cast<InvEntry*>(me);
    ASSERT(entry);

    if (entry->item->base_type == OBJ_RUNES)
    {
        auto rune = static_cast<rune_type>(entry->item->sub_type);
        colour_t colour;
        // Make Gloorx's rune more distinguishable from uncollected runes.
        if (you.runes[rune])
        {
            colour = (rune == RUNE_GLOORX_VLOQ) ? colour_t{LIGHTGREY}
                                                : rune_colour(rune);
        }
        else
            colour = DARKGREY;

        string text = "<";
        text += colour_to_str(colour);
        text += ">";
        text += rune_type_name(rune);
        text += " rune of Zot";
        if (!you.runes[rune])
        {
            text += " (";
            text += branches[rune_location(rune)].longname;
            text += ")";
        }
        text += "</";
        text += colour_to_str(colour);
        text += ">";
        entry->text = text;
    }
    else if (entry->item->is_type(OBJ_ORBS, ORB_ZOT))
    {
        if (player_has_orb())
            entry->text = gettext_noop("<magenta>The Orb of Zot</magenta>");
        else
        {
            entry->text = gettext_noop("<darkgrey>The Orb of Zot"
                          " (the Realm of Zot)</darkgrey>");
        }
    }

    return entry;
}

void display_runes()
{
    auto col = runes_in_pack() < ZOT_ENTRY_RUNES ?  gettext_noop("lightgrey") :
               runes_in_pack() < you.obtainable_runes ? gettext_noop("green") :
                                                   gettext_noop("lightgreen");

    auto title = make_stringf(gettext_noop("<white>Runes of Zot (</white>"
                              "<%s>%d</%s><white> collected) & Orbs of Power</white>"),
                              col, runes_in_pack(), col);

    InvMenu menu(MF_NOSELECT | MF_ALLOW_FORMATTING);

    menu.set_title(title);

    vector<item_def> items;

    if (!crawl_state.game_is_sprint())
    {
        // Add the runes in order of challenge (semi-arbitrary).
        for (branch_iterator it(branch_iterator_type::danger); it; ++it)
        {
            const branch_type br = it->id;
            if (!connected_branch_can_exist(br))
                continue;

            for (auto rune : branches[br].runes)
            {
                item_def item;
                item.base_type = OBJ_RUNES;
                item.sub_type = rune;
                item.quantity = you.runes[rune] ? 1 : 0;
                item_colour(item);
                items.push_back(item);
            }
        }
    }
    else
    {
        // We don't know what runes are accessible in the sprint, so just show
        // the ones you have. We can't iterate over branches as above since the
        // elven rune and mossy rune may exist in sprint.
        for (int i = 0; i < NUM_RUNE_TYPES; ++i)
        {
            if (you.runes[i])
            {
                item_def item;
                item.base_type = OBJ_RUNES;
                item.sub_type = i;
                item.quantity = 1;
                item_colour(item);
                items.push_back(item);
            }
        }
    }
    item_def item;
    item.base_type = OBJ_ORBS;
    item.sub_type = ORB_ZOT;
    item.quantity = player_has_orb() ? 1 : 0;
    items.push_back(item);

    // We've sorted this vector already, so disable menu sorting. Maybe we
    // could a menu entry comparator and modify InvMenu::load_items() to allow
    // passing this in instead of doing a sort ahead of time.
    menu.load_items(items, _fixup_runeorb_entry, 0, false);

    menu.show();
}

// Seed ranges for _random_consonant_set: (B)eginning and one-past-the-(E)nd
// of the (B)eginning, (E)nding, and (M)iddle cluster ranges.
const size_t RCS_BB = 0;
const size_t RCS_EB = 27;
const size_t RCS_BE = 14;
const size_t RCS_EE = 56;
const size_t RCS_BM = 0;
const size_t RCS_EM = 67;
const size_t RCS_END = RCS_EM;

#define ITEMNAME_SIZE 200
/**
 * Make a random name from the given seed.
 *
 * Used for: Pandemonium demonlords, shopkeepers, scrolls, random artefacts.
 *
 * This function is insane, but that might be useful.
 *
 * @param seed      The seed to generate the name from.
 *                  The same seed will always generate the same name.
 *                  By default a random number from the current RNG.
 * @param name_type The type of name to be generated.
 *                  If MNAME_SCROLL, increase length by 6 and force to allcaps.
 *                  If MNAME_JIYVA, start with J, do not generate spaces,
 *                  recurse instead of ploggifying, and cap length at 8.
 *                  Otherwise, no special behaviour.
 * @return          A randomly generated name.
 *                  E.g. "Joiduir", "Jays Fya", ZEFOKY WECZYXE,
 *                  THE GIAGGOSTUONO, etc.
 */
string make_name(uint32_t seed, makename_type name_type)
{
    // use the seed to select sequence, rather than seed per se. This is
    // because it is not important that the sequence be randomly distributed
    // in uint64_t.
    rng::subgenerator subgen(you.game_seed, static_cast<uint64_t>(seed));

    string name;

    bool has_space  = false; // Keep track of whether the name contains a space.

    size_t len = 3;
    len += random2(5);
    len += (random2(5) == 0) ? random2(6) : 1;

    if (name_type == MNAME_SCROLL)   // scrolls have longer names
        len += 6;

    const size_t maxlen = name_type == MNAME_JIYVA ? 8 : SIZE_MAX;
    len = min(len, maxlen);

    ASSERT_RANGE(len, 1, ITEMNAME_SIZE + 1);

    static const int MAX_ITERS = 150;
    for (int iters = 0; iters < MAX_ITERS && name.length() < len; ++iters)
    {
        const char prev_char = name.length() ? name[name.length() - 1]
                                              : '\0';
        const char penult_char = name.length() > 1 ? name[name.length() - 2]
                                                    : '\0';
        if (name.empty() && name_type == MNAME_JIYVA)
        {
            // Start the name with a predefined letter.
            name += 'j';
        }
        else if (name.empty() || prev_char == ' ')
        {
            // Start the word with any letter.
            name += 'a' + random2(26);
        }
        else if (!has_space && name_type != MNAME_JIYVA
                 && name.length() > 5 && name.length() < len - 4
                 && random2(5) != 0) // 4/5 chance
        {
             // Hand out a space.
            name += ' ';
        }
        else if (name.length()
                 && (_is_consonant(prev_char)
                     || (name.length() > 1
                         && !_is_consonant(prev_char)
                         && _is_consonant(penult_char)
                         && random2(5) <= 1))) // 2/5
        {
            // Place a vowel.
            const char vowel = _random_vowel();

            if (vowel == ' ')
            {
                if (len < 7
                         || name.length() <= 2 || name.length() >= len - 3
                         || prev_char == ' ' || penult_char == ' '
                         || name_type == MNAME_JIYVA
                         || name.length() > 2
                            && _is_consonant(prev_char)
                            && _is_consonant(penult_char))
                {
                    // Replace the space with something else if ...
                    // * the name is really short
                    // * we're close to the start/end of the name
                    // * we just got a space
                    // * we're generating a jiyva name, or
                    // * the last two letters were consonants
                    continue;
                }
            }
            else if (name.length() > 1
                     && vowel == prev_char
                     && (vowel == 'y' || vowel == 'i'
                         || random2(5) <= 1))
            {
                // Replace the vowel with something else if the previous
                // letter was the same, and it's a 'y', 'i' or with 2/5 chance.
                continue;
            }

            name += vowel;
        }
        else // We want a consonant.
        {
            // Are we at start or end of the (sub) name?
            const bool beg = (name.empty() || prev_char == ' ');
            const bool end = (name.length() >= len - 2);

            // Use one of number of predefined letter combinations.
            if ((len > 3 || !name.empty())
                && random2(7) <= 1 // 2/7 chance
                && (!beg || !end))
            {
                const int first = (beg ? RCS_BB : (end ? RCS_BE : RCS_BM));
                const int last  = (beg ? RCS_EB : (end ? RCS_EE : RCS_EM));

                const int range = last - first;

                const int cons_seed = random2(range) + first;

                const string consonant_set = _random_consonant_set(cons_seed);

                ASSERT(consonant_set.size() > 1);
                len += consonant_set.size() - 2; // triples increase len
                name += consonant_set;
            }
            else // Place a single letter instead.
            {
                // Pick a random consonant.
                name += _random_cons();
            }
        }

        if (name[name.length() - 1] == ' ')
        {
            ASSERT(name_type != MNAME_JIYVA);
            has_space = true;
        }
    }

    // Catch early exit and try to give a final letter.
    const char last_char = name[name.length() - 1];
    if (!name.empty()
        && last_char != ' '
        && last_char != 'y'
        && !_is_consonant(name[name.length() - 1])
        && (name.length() < len    // early exit
            || (len < 8
                && random2(3) != 0))) // 2/3 chance for other short names
    {
        // Specifically, add a consonant.
        name += _random_cons();
    }

    if (maxlen != SIZE_MAX)
        name = chop_string(name, maxlen);
    trim_string_right(name);

    // Fallback if the name was too short.
    if (name.length() < 4)
    {
        // convolute & recurse
        if (name_type == MNAME_JIYVA)
            return make_name(rng::get_uint32(), MNAME_JIYVA);

        name = gettext_noop("plog");
    }

    string uppercased_name;
    for (size_t i = 0; i < name.length(); i++)
    {
        if (name_type == MNAME_JIYVA)
            ASSERT(name[i] != ' ');

        if (name_type == MNAME_SCROLL || i == 0 || name[i - 1] == ' ')
            uppercased_name += toupper_safe(name[i]);
        else
            uppercased_name += name[i];
    }

    return uppercased_name;
}
#undef ITEMNAME_SIZE

/**
 * Is the given character a lower-case ascii consonant?
 *
 * For our purposes, y is not a consonant.
 */
static bool _is_consonant(char let)
{
    static const set<char> all_consonants = { 'b', 'c', 'd', 'f', 'g',
                                              'h', 'j', 'k', 'l', 'm',
                                              'n', 'p', 'q', 'r', 's',
                                              't', 'v', 'w', 'x', 'z' };
    return all_consonants.count(let);
}

// Returns a random vowel (a, e, i, o, u with equal probability) or space
// or 'y' with lower chances.
static char _random_vowel()
{
    static const char vowels[] = "aeiouaeiouaeiouy  ";
    return vowels[random2(sizeof(vowels) - 1)];
}

// Returns a random consonant with not quite equal probability.
// Does not include 'y'.
static char _random_cons()
{
    static const char consonants[] = "bcdfghjklmnpqrstvwxzcdfghlmnrstlmnrst";
    return consonants[random2(sizeof(consonants) - 1)];
}

/**
 * Choose a random consonant tuple/triple, based on the given seed.
 *
 * @param seed  The index into the consonant array; different seed ranges are
 *              expected to correspond with the place in the name being
 *              generated where the consonants should be inserted.
 * @return      A random length 2 or 3 consonant set; e.g. "kl", "str", etc.
 *              If the seed is out of bounds, return "";
 */
static string _random_consonant_set(size_t c)
{
    // Pick a random combination of consonants from the set below.
    //   begin  -> [RCS_BB, RCS_EB) = [ 0, 27)
    //   middle -> [RCS_BM, RCS_EM) = [ 0, 67)
    //   end    -> [RCS_BE, RCS_EE) = [14, 56)

    static const string consonant_sets[] = {
        // 0-13: start, middle
        "kl", "gr", "cl", "cr", "fr",
        "pr", "tr", "tw", "br", "pl",
        "bl", "str", "shr", "thr",
        // 14-26: start, middle, end
        "sm", "sh", "ch", "th", "ph",
        "pn", "kh", "gh", "mn", "ps",
        "st", "sk", "sch",
        // 27-55: middle, end
        "ts", "cs", "xt", "nt", "ll",
        "rr", "ss", "wk", "wn", "ng",
        "cw", "mp", "ck", "nk", "dd",
        "tt", "bb", "pp", "nn", "mm",
        "kk", "gg", "ff", "pt", "tz",
        "dgh", "rgh", "rph", "rch",
        // 56-66: middle only
        "cz", "xk", "zx", "xz", "cv",
        "vv", "nl", "rh", "dw", "nw",
        "khl",
    };
    COMPILE_CHECK(ARRAYSZ(consonant_sets) == RCS_END);

    ASSERT_RANGE(c, 0, ARRAYSZ(consonant_sets));

    return consonant_sets[c];
}

/**
 * Write all possible scroll names to the given file.
 */
static void _test_scroll_names(const string& fname)
{
    FILE *f = fopen(fname.c_str(), "w");
    if (!f)
        sysfail("can't write test output");

    string longest;
    for (int i = 0; i < 151; i++)
    {
        for (int j = 0; j < 151; j++)
        {
            const int seed = i | (j << 8) | (OBJ_SCROLLS << 16);
            const string name = make_name(seed, MNAME_SCROLL);
            if (name.length() > longest.length())
                longest = name;
            fprintf(f, "%s\n", name.c_str());
        }
    }

    fprintf(f, "\nLongest: %s (%d)\n", longest.c_str(), (int)longest.length());

    fclose(f);
}

/**
 * Write one million random Jiyva names to the given file.
 */
static void _test_jiyva_names(const string& fname)
{
    FILE *f = fopen(fname.c_str(), "w");
    if (!f)
        sysfail("can't write test output");

    string longest;
    rng::seed(27);
    for (int i = 0; i < 1000000; i++)
    {
        const string name = make_name(rng::get_uint32(), MNAME_JIYVA);
        ASSERT(name[0] == 'J');
        if (name.length() > longest.length())
            longest = name;
        fprintf(f, "%s\n", name.c_str());
    }

    fprintf(f, "\nLongest: %s (%d)\n", longest.c_str(), (int)longest.length());

    fclose(f);
}

/**
 * Test make_name().
 *
 * Currently just a stress test iterating over all possible scroll names.
 */
void make_name_tests()
{
    _test_jiyva_names("jiyva_names.out");
    _test_scroll_names("scroll_names.out");

    rng::seed(27);
    for (int i = 0; i < 1000000; ++i)
        make_name();
}

bool is_interesting_item(const item_def& item)
{
    if (fully_identified(item) && is_artefact(item))
        return true;

    const string iname = item_prefix(item, false) + " " + item.name(DESC_PLAIN);
    for (const text_pattern &pat : Options.note_items)
        if (pat.matches(iname))
            return true;

    return false;
}

/**
 * Is an item a potentially life-saving consumable in emergency situations?
 * Unlike similar functions, this one never takes temporary conditions into
 * account. It does, however, take religion and mutations into account.
 * Permanently unusable items are in general not considered emergency items.
 *
 * @param item The item being queried.
 * @return True if the item is known to be an emergency item.
 */
bool is_emergency_item(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_TELEPORTATION:
        case SCR_BLINKING:
            return !you.stasis();
        case SCR_FEAR:
        case SCR_FOG:
            return true;
        default:
            return false;
        }
    case OBJ_POTIONS:
        if (!you.can_drink())
            return false;

        switch (item.sub_type)
        {
        case POT_HASTE:
            return !have_passive(passive_t::no_haste)
                && !you.stasis();
        case POT_HEAL_WOUNDS:
            return you.can_potion_heal();
        case POT_CURING:
        case POT_RESISTANCE:
        case POT_MAGIC:
            return true;
        default:
            return false;
        }
    case OBJ_MISSILES:
        // Missiles won't help Felids.
        if (you.has_mutation(MUT_NO_GRASPING))
            return false;

        switch (item.sub_type)
        {
        case MI_DART:
            return get_ammo_brand(item) == SPMSL_CURARE
                   || get_ammo_brand(item) == SPMSL_BLINDING;
        case MI_BOOMERANG:
            return get_ammo_brand(item) == SPMSL_DISPERSAL;
        case MI_THROWING_NET:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

/**
 * Is an item a particularly good consumable? Unlike similar functions,
 * this one never takes temporary conditions into account. Permanently
 * unusable items are in general not considered good.
 *
 * @param item The item being queried.
 * @return True if the item is known to be good.
 */
bool is_good_item(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    if (is_emergency_item(item))
        return true;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        if (item.sub_type == SCR_TORMENT)
            return you.res_torment();
        return item.sub_type == SCR_ACQUIREMENT;
    case OBJ_POTIONS:
        if (!you.can_drink(false)) // still want to pick them up in lichform?
            return false;
        switch (item.sub_type)
        {
        case POT_EXPERIENCE:
            return true;
        default:
            return false;
        CASE_REMOVED_POTIONS(item.sub_type)
        }
    default:
        return false;
    }
}

/**
 * Is an item strictly harmful?
 *
 * @param item The item being queried.
 * @return True if the item is known to have only harmful effects.
 */
bool is_bad_item(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case SCR_CURSE_ARMOUR:
            if (you.has_mutation(MUT_NO_ARMOUR))
                return false;
        case SCR_CURSE_WEAPON:
            if (you.has_mutation(MUT_NO_GRASPING))
                return false;
        case SCR_CURSE_JEWELLERY:
            return !have_passive(passive_t::want_curses);
#endif
        case SCR_NOISE:
            return true;
        default:
            return false;
        }
    case OBJ_POTIONS:
        // Can't be bad if you can't use them.
        if (!you.can_drink(false))
            return false;

        switch (item.sub_type)
        {
        case POT_DEGENERATION:
            return true;
        default:
            return false;
        CASE_REMOVED_POTIONS(item.sub_type);
        }
    case OBJ_JEWELLERY:
        // Potentially useful. TODO: check the properties.
        if (is_artefact(item))
            return false;

        switch (item.sub_type)
        {
        case RING_EVASION:
        case RING_PROTECTION:
        case RING_STRENGTH:
        case RING_DEXTERITY:
        case RING_INTELLIGENCE:
        case RING_SLAYING:
            return item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus <= 0;
        default:
            return false;
        }

    default:
        return false;
    }
}

/**
 * Is an item dangerous but potentially worthwhile?
 *
 * @param item The item being queried.
 * @param temp Should temporary conditions such as transformations and
 *             vampire state be taken into account?  Religion (but
 *             not its absence) is considered to be permanent here.
 * @return True if using the item is known to be risky but occasionally
 *         worthwhile.
 */
bool is_dangerous_item(const item_def &item, bool temp)
{
    if (!item_type_known(item))
        return false;

    // useless items can hardly be dangerous.
    if (is_useless_item(item, temp))
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_IMMOLATION:
        case SCR_VULNERABILITY:
            return true;
        case SCR_TORMENT:
            return !you.res_torment();
        case SCR_HOLY_WORD:
            return you.undead_or_demonic();
        default:
            return false;
        }

    case OBJ_POTIONS:
        switch (item.sub_type)
        {
        case POT_MUTATION:
            if (have_passive(passive_t::cleanse_mut_potions))
                return false;
            // intentional fallthrough
        case POT_LIGNIFY:
        case POT_ATTRACTION:
            return true;
        default:
            return false;
        }

    case OBJ_MISCELLANY:
        // Tremorstones will blow you right up.
        return item.sub_type == MISC_TIN_OF_TREMORSTONES;

    case OBJ_ARMOUR:
        if (you.get_mutation_level(MUT_NO_LOVE)
            && is_unrandom_artefact(item, UNRAND_RATSKIN_CLOAK))
        {
            // some people don't like being randomly attacked by rats.
            // weird but what can you do.
            return true;
        }

        // Tilting at windmills can be dangerous.
        return get_armour_ego_type(item) == SPARM_RAMPAGING;

    default:
        return false;
    }
}

static bool _invisibility_is_useless(const bool temp)
{
    // If you're Corona'd or a TSO-ite, this is always useless.
    return temp ? you.backlit()
                : you.haloed() && will_have_passive(passive_t::halo);
}

/**
 * Is an item (more or less) useless to the player? Uselessness includes
 * but is not limited to situations such as:
 * \li The item cannot be used.
 * \li Using the item would have no effect.
 * \li Using the item would have purely negative effects (<tt>is_bad_item</tt>).
 * \li Using the item is expected to produce no benefit for a player of their
 *     religious standing. For example, magic enhancers for Trog worshippers
 *     are "useless", even if the player knows a spell and therefore could
 *     benefit.
 *
 * @param item The item being queried.
 * @param temp Should temporary conditions such as transformations and
 *             vampire state be taken into account? Religion (but
 *             not its absence) is considered to be permanent here.
 * @param ident Should uselessness be checked as if the item were already
 *              identified?
 * @return True if the item is known to be useless.
 */
bool is_useless_item(const item_def &item, bool temp, bool ident)
{
    // During game startup, no item is useless. If someone re-glyphs an item
    // based on its uselessness, the glyph-to-item cache will use the useless
    // value even if your god or species can make use of it.
    if (you.species == SP_UNKNOWN)
        return false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (you.has_mutation(MUT_NO_GRASPING))
            return true;

        if (!you.could_wield(item, true, !temp)
            && !is_throwable(&you, item))
        {
            // Weapon is too large (or small) to be wielded and cannot
            // be thrown either.
            return true;
        }

        if (you.undead_or_demonic() && is_holy_item(item, false))
        {
            if (!temp && you.form == transformation::lich
                && you.species != SP_DEMONSPAWN)
            {
                return false;
            }
            return true;
        }

        return false;

    case OBJ_MISSILES:
        if ((you.has_spell(SPELL_SANDBLAST)
                || !you.num_turns && you.char_class == JOB_EARTH_ELEMENTALIST)
                && item.sub_type == MI_STONE)
        {
            return false;
        }

        // Save for the above spell, all missiles are useless for felids.
        if (you.has_mutation(MUT_NO_GRASPING))
            return true;

        // These are the same checks as in is_throwable(), except that
        // we don't take launchers into account.
        switch (item.sub_type)
        {
        case MI_LARGE_ROCK:
            return !you.can_throw_large_rocks();
        case MI_JAVELIN:
            return you.body_size(PSIZE_BODY, !temp) < SIZE_MEDIUM
                   && !you.can_throw_large_rocks();
        }

        return false;

    case OBJ_ARMOUR:
        if (!can_wear_armour(item, false, true))
            return true;

        if (is_shield(item) && you.get_mutation_level(MUT_MISSING_HAND))
            return true;

        if (is_artefact(item))
            return false;

        if (item.sub_type == ARM_SCARF && (ident || item_type_known(item)))
        {
            special_armour_type ego = get_armour_ego_type(item);
            switch (ego)
            {
            case SPARM_SPIRIT_SHIELD:
                return you.spirit_shield(false, false);
            case SPARM_REPULSION:
                return temp && have_passive(passive_t::upgraded_storm_shield)
                       || you.get_mutation_level(MUT_DISTORTION_FIELD) == 3;
            case SPARM_INVISIBILITY:
                return you.has_mutation(MUT_NO_ARTIFICE);
            default:
                return false;
            }
        }
        return false;

    case OBJ_SCROLLS:
        if (temp && silenced(you.pos()))
            return true; // can't use scrolls while silenced

        if (!ident && !item_type_known(item))
            return false;

        // A bad item is always useless.
        if (is_bad_item(item))
            return true;

        switch (item.sub_type)
        {
        case SCR_TELEPORTATION:
            return you.stasis()
                   || crawl_state.game_is_sprint()
                   || temp && player_in_branch(BRANCH_GAUNTLET);
        case SCR_BLINKING:
            return you.stasis();
        case SCR_AMNESIA:
            return you_worship(GOD_TROG) || you.has_mutation(MUT_INNATE_CASTER);
#if TAG_MAJOR_VERSION == 34
        case SCR_CURSE_WEAPON: // for non-Ashenzari, already handled
        case SCR_CURSE_ARMOUR:
#endif
        case SCR_ENCHANT_WEAPON:
        case SCR_ENCHANT_ARMOUR:
        case SCR_BRAND_WEAPON:
            return you.has_mutation(MUT_NO_GRASPING);
        case SCR_SUMMONING:
            return you.get_mutation_level(MUT_NO_LOVE) > 0;
        case SCR_FOG:
            return temp && (env.level_state & LSTATE_STILL_WINDS);
        case SCR_IDENTIFY:
            return you_worship(GOD_ASHENZARI);
        default:
            return false;
        }

    case OBJ_WANDS:
        if (you.get_mutation_level(MUT_NO_ARTIFICE))
            return true;

#if TAG_MAJOR_VERSION == 34
        if (is_known_empty_wand(item))
            return true;
#endif
        if (!ident && !item_type_known(item))
            return false;

        if (item.sub_type == WAND_CHARMING)
            return you.get_mutation_level(MUT_NO_LOVE);

        return false;

    case OBJ_POTIONS:
    {
        // Mummies and liches can't use potions.
        if (!you.can_drink(temp))
            return true;

        if (!ident && !item_type_known(item))
            return false;

        // A bad item is always useless.
        if (is_bad_item(item))
            return true;

        switch (item.sub_type)
        {
        case POT_BERSERK_RAGE:
            return !you.can_go_berserk(true, true, true, nullptr, temp);
        case POT_HASTE:
            return you.stasis();
        case POT_MUTATION:
            return !you.can_safely_mutate(temp);
        case POT_LIGNIFY:
            return you.is_lifeless_undead(temp);
        case POT_FLIGHT:
            return you.permanent_flight();
        case POT_HEAL_WOUNDS:
            return !you.can_potion_heal();
        case POT_INVISIBILITY:
            return _invisibility_is_useless(temp);
        case POT_BRILLIANCE:
            return you_worship(GOD_TROG);
        case POT_MAGIC:
            return you.has_mutation(MUT_HP_CASTING);
        CASE_REMOVED_POTIONS(item.sub_type)
        }

        return false;
    }
    case OBJ_JEWELLERY:
        if (!ident && !item_type_known(item))
            return false;

        // Potentially useful. TODO: check the properties.
        if (is_artefact(item))
            return false;

        if (is_bad_item(item))
            return true;

        switch (item.sub_type)
        {
        case RING_RESIST_CORROSION:
            return you.res_corr(false, false);

        case AMU_FAITH:
            return (you.has_mutation(MUT_FORLORN) && !you.religion) // ??
                    || you_worship(GOD_GOZAG) || you_worship(GOD_ASHENZARI)
                    || (you_worship(GOD_RU) && you.piety == piety_breakpoint(5));

        case AMU_GUARDIAN_SPIRIT:
            return you.spirit_shield(false, false) || you.has_mutation(MUT_HP_CASTING);

        case RING_LIFE_PROTECTION:
            return player_prot_life(false, temp, false) == 3;

        case AMU_REGENERATION:
            return you.get_mutation_level(MUT_NO_REGENERATION) > 0
                   || (temp
                       && (you.get_mutation_level(MUT_INHIBITED_REGENERATION) > 0
                           || you.has_mutation(MUT_VAMPIRISM))
                       && regeneration_is_inhibited());

        case AMU_MANA_REGENERATION:
#if TAG_MAJOR_VERSION == 34
            if (have_passive(passive_t::no_mp_regen)
                || player_under_penance(GOD_PAKELLAS))
            {
                return true;
            }
#endif
            return !you.max_magic_points;

        case RING_MAGICAL_POWER:
            return you.has_mutation(MUT_HP_CASTING);

        case RING_SEE_INVISIBLE:
            return you.innate_sinv();

        case RING_POISON_RESISTANCE:
            return player_res_poison(false, temp, false) > 0;

        case RING_WIZARDRY:
            return you_worship(GOD_TROG);

        case RING_FLIGHT:
            return you.permanent_flight(false);

        case RING_STEALTH:
            return you.get_mutation_level(MUT_NO_STEALTH);

        default:
            return false;
        }

#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
            return true;
#endif

    case OBJ_STAVES:
        if (you.has_mutation(MUT_NO_GRASPING))
            return true;
        if (!you.could_wield(item, true, !temp))
        {
            // Weapon is too large (or small) to be wielded and cannot
            // be thrown either.
            return true;
        }
        if (!ident && !item_type_known(item))
            return false;

        return false;

    case OBJ_CORPSES:
        if (you.has_spell(SPELL_ANIMATE_DEAD)
            || you.has_spell(SPELL_ANIMATE_SKELETON)
            || you.has_spell(SPELL_SIMULACRUM)
            || you_worship(GOD_YREDELEMNUL) && !you.penance[GOD_YREDELEMNUL]
               && you.piety >= piety_breakpoint(0))
        {
            return false;
        }

        return true;

    case OBJ_MISCELLANY:
        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case MISC_BUGGY_EBONY_CASKET:
            return item_type_known(item);
#endif
        // These can always be used.
#if TAG_MAJOR_VERSION == 34
        case MISC_BUGGY_LANTERN_OF_SHADOWS:
#endif
        case MISC_ZIGGURAT:
            return false;

        // Purely summoning misc items don't work w/ sac love
        case MISC_BOX_OF_BEASTS:
        case MISC_HORN_OF_GERYON:
        case MISC_PHANTOM_MIRROR:
            return you.get_mutation_level(MUT_NO_LOVE)
                   || you.get_mutation_level(MUT_NO_ARTIFICE);

        case MISC_CONDENSER_VANE:
            if (temp && (env.level_state & LSTATE_STILL_WINDS))
                return true;
            // Intentional fallthrough to check artifice

        default:
            return you.get_mutation_level(MUT_NO_ARTIFICE);
        }

    case OBJ_BOOKS:
        // this might be wrong if we ever add more item ID back
        if (you.has_mutation(MUT_INNATE_CASTER) && item.sub_type != BOOK_MANUAL)
            return true;
        if (!ident && !item_type_known(item))
            return false;
        if ((ident || item_type_known(item)) && item.sub_type != BOOK_MANUAL)
        {
            // Spellbooks are useless if all spells are either in the library
            // already or are uncastable.
            bool useless = true;
            for (spell_type st : spells_in_book(item))
                if (!you.spell_library[st] && you_can_memorise(st))
                    useless = false;
            return useless;
        }
        // If we're here, it's a manual.
        if (you.skills[item.plus] >= 27)
            return true;
        return is_useless_skill((skill_type)item.plus);

    default:
        return false;
    }
    return false;
}

string item_prefix(const item_def &item, bool temp)
{
    vector<const char *> prefixes;

    if (!item.defined())
        return "";

    if (fully_identified(item))
        prefixes.push_back(gettext_noop("identified"));
    else if (item_ident(item, ISFLAG_KNOW_TYPE)
             || get_ident_type(item))
    {
        prefixes.push_back(gettext_noop("known"));
    }
    else
        prefixes.push_back(gettext_noop("unidentified"));

    if (god_hates_item(item))
    {
        prefixes.push_back(gettext_noop("evil_item"));
        prefixes.push_back(gettext_noop("forbidden"));
    }

    if (is_emergency_item(item))
        prefixes.push_back(gettext_noop("emergency_item"));
    if (is_good_item(item))
        prefixes.push_back(gettext_noop("good_item"));
    if (is_dangerous_item(item, temp))
        prefixes.push_back(gettext_noop("dangerous_item"));
    if (is_bad_item(item))
        prefixes.push_back(gettext_noop("bad_item"));
    if (is_useless_item(item, temp))
        prefixes.push_back(gettext_noop("useless_item"));

    if (item_is_stationary(item))
        prefixes.push_back(gettext_noop("stationary"));

    if (!is_artefact(item) && (item.base_type == OBJ_WEAPONS
                               || item.base_type == OBJ_ARMOUR))
    {
        if (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus > 0)
            prefixes.push_back(gettext_noop("enchanted"));
        if (item_ident(item, ISFLAG_KNOW_TYPE) && item.brand)
            prefixes.push_back(gettext_noop("ego"));
    }

    switch (item.base_type)
    {
    case OBJ_STAVES:
    case OBJ_WEAPONS:
        if (is_range_weapon(item))
            prefixes.push_back(gettext_noop("ranged"));
        else if (is_melee_weapon(item)) // currently redundant
            prefixes.push_back(gettext_noop("melee"));
        // fall through

    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (is_artefact(item))
            prefixes.push_back(gettext_noop("artefact"));
        // fall through

    case OBJ_MISSILES:
        if (item_is_equipped(item, true))
            prefixes.push_back(gettext_noop("equipped"));
        break;

    case OBJ_BOOKS:
        if (item.sub_type != BOOK_MANUAL && item.sub_type != NUM_BOOKS)
            prefixes.push_back(gettext_noop("spellbook"));
        break;

    default:
        break;
    }

    prefixes.push_back(item_class_name(item.base_type, true));

    string result = comma_separated_line(prefixes.begin(), prefixes.end(),
                                         " ", " ");

    return result;
}

/**
 * Return an item's name surrounded by colour tags, using menu colouring
 *
 * @param item The item being queried
 * @param desc The description level to use for the name string
 * @return A string containing the item's name surrounded by colour tags
 */
string menu_colour_item_name(const item_def &item, description_level_type desc)
{
    const string cprf      = item_prefix(item);
    const string item_name = item.name(desc);

    const int col = menu_colour(item_name, cprf, gettext_noop("pickup"));
    if (col == -1)
        return item_name;

    const string colour = colour_to_str(col);
    const char * const colour_z = colour.c_str();
    return make_stringf(gettext_noop("<%s>%s</%s>"), colour_z, item_name.c_str(), colour_z);
}

typedef map<string, item_kind> item_names_map;
static item_names_map item_names_cache;

typedef map<unsigned, vector<string> > item_names_by_glyph_map;
static item_names_by_glyph_map item_names_by_glyph_cache;

void init_item_name_cache()
{
    item_names_cache.clear();
    item_names_by_glyph_cache.clear();

    for (int i = 0; i < NUM_OBJECT_CLASSES; i++)
    {
        const object_class_type base_type = static_cast<object_class_type>(i);

        for (const auto sub_type : all_item_subtypes(base_type))
        {
            if (base_type == OBJ_BOOKS)
            {
                if (sub_type == BOOK_RANDART_LEVEL
                    || sub_type == BOOK_RANDART_THEME)
                {
                    // These are randart only and have no fixed names.
                    continue;
                }
            }

            int npluses = 0;
            if (base_type == OBJ_BOOKS && sub_type == BOOK_MANUAL)
                npluses = NUM_SKILLS;

            item_def item;
            item.base_type = base_type;
            item.sub_type = sub_type;
            for (int plus = 0; plus <= npluses; plus++)
            {
                if (plus > 0)
                    item.plus = max(0, plus - 1);
                string name = item.name(plus || item.base_type == OBJ_RUNES ? DESC_PLAIN : DESC_DBNAME,
                                        true, true);
                lowercase(name);
                cglyph_t g = get_item_glyph(item);

                if (base_type == OBJ_JEWELLERY && sub_type >= NUM_RINGS
                    && sub_type < AMU_FIRST_AMULET)
                {
                    continue;
                }
                else if (name.find(gettext_noop("buggy")) != string::npos)
                {
                    mprf(MSGCH_ERROR, gettext_noop("Bad name for item name cache: %s"),
                                                                name.c_str());
                    continue;
                }

                if (!item_names_cache.count(name))
                {
                    item_names_cache[name] = { base_type, (uint8_t)sub_type,
                                               (int8_t)item.plus, 0 };
                    if (g.ch)
                        item_names_by_glyph_cache[g.ch].push_back(name);
                }
            }
        }
    }

    ASSERT(!item_names_cache.empty());
}

item_kind item_kind_by_name(const string &name)
{
    return lookup(item_names_cache, lowercase_string(name),
                  { OBJ_UNASSIGNED, 0, 0, 0 });
}

vector<string> item_name_list_for_glyph(char32_t glyph)
{
    return lookup(item_names_by_glyph_cache, glyph, {});
}

bool is_named_corpse(const item_def &corpse)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    return corpse.props.exists(CORPSE_NAME_KEY);
}

string get_corpse_name(const item_def &corpse, monster_flags_t *name_type)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);

    if (!corpse.props.exists(CORPSE_NAME_KEY))
        return "";

    if (name_type != nullptr)
        name_type->flags = corpse.props[CORPSE_NAME_TYPE_KEY].get_int64();

    return corpse.props[CORPSE_NAME_KEY].get_string();
}
