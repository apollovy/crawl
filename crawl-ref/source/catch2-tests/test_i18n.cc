//
// Created by Аполлов Юрий Андреевич on 05.05.2021.
//

#include "catch.hpp"

#include "AppHdr.h"

#include "branch.h"
#include "crawl_locale.h"
#include "database.h"
#include "directn.h"
#include "initfile.h"
#include "mon-info.h"
#include "options.h"
#include "terrain.h"


#define RUSSIAN() \
    setup_russian(); \
    SECTION("ru")

#define DEFAULT() \
    setup_c(); \
    SECTION("c")

#define setup_russian() \
    Options.lang_name = "ru"; \
    Options.language = lang_t::RU; \
    setup_language("ru_RU.UTF-8");

#define setup_c() \
    Options.lang_name = ""; \
    Options.language = lang_t::EN; \
    setup_language("C");

#define CRAWL_I18N_TEST_CASE_START(...) \
    TEST_CASE(__VA_ARGS__) { \
    bindtextdomain("crawl", "./lang/mo"); \
    textdomain("crawl"); \
    SysEnv.crawl_base = "./dat"; \
    init_show_table();

#define CRAWL_I18N_TEST_CASE_END(...) \
    setup_c(); \
    }


void setup_language(string lang) {
    setlocale(LC_ALL, lang.c_str());
    putenv(strdup(("LANGUAGE=" + lang).c_str()));
    databaseSystemInit();
}


CRAWL_I18N_TEST_CASE_START("Test various i18n contexts")
    SECTION("no context") {
        SECTION("jackal") {
            auto mi = monster_info(MONS_JACKAL);
            RUSSIAN() {
                REQUIRE(mi.common_name() == "шакал");
            }
            DEFAULT() {
                REQUIRE(mi.common_name() == "jackal");
            }
        }

        SECTION("jackal zombie") {
            monster_info mi = monster_info(MONS_ZOMBIE, MONS_JACKAL);
            RUSSIAN() {
                REQUIRE(mi.common_name() == "шакал зомби");
            }
            DEFAULT() {
                REQUIRE(mi.common_name() == "jackal zombie");
            }
        }

        SECTION("bad 'a %s'") {
            RUSSIAN() {
                REQUIRE(string(_("a %s")) == "%s");
            }
            DEFAULT() {
                REQUIRE(string(_("a %s")) == "a %s");
            }
        }
    }
CRAWL_I18N_TEST_CASE_END("Test various i18n contexts")


CRAWL_I18N_TEST_CASE_START("Test descriptions work")
    SECTION("stone staircase leading down") {
        RUSSIAN() {
            auto stairs = feature_description(DNGN_STONE_STAIRS_DOWN_I);
            REQUIRE(getLongDescription(stairs) == "Эта лестница ведет вниз.\n");
        }
        DEFAULT() {
            auto stairs = feature_description(DNGN_STONE_STAIRS_DOWN_I);
            REQUIRE(getLongDescription(stairs) == "A staircase leading further down.\n");
        }
    }
CRAWL_I18N_TEST_CASE_END("Test various i18n contexts")

CRAWL_I18N_TEST_CASE_START("Test i18n context macros works as expected")
    REQUIRE(i18n_cnames[I18NC_MELEE_ATTACKER] == "%s bites you for 10 damage with +3 dagger!!!");
CRAWL_I18N_TEST_CASE_END("Test i18n context macros works as expected")
