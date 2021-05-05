//
// Created by Аполлов Юрий Андреевич on 05.05.2021.
//

#include "catch.hpp"

#include "AppHdr.h"
#include "branch.h"
#include "mon-info.h"
#include "crawl_locale.h"

TEST_CASE( "Test various i18n contexts in Russian" ) {
    setlocale(LC_ALL, "ru_RU.UTF-8");
    bindtextdomain("crawl", "./lang/mo");
    textdomain("crawl");

    SECTION ("jackal") {
        monster_info mi = monster_info(MONS_JACKAL);
        REQUIRE( mi.common_name() == "шакал" );
    }

    SECTION ("jackal zombie") {
        monster_info mi = monster_info(MONS_ZOMBIE, MONS_JACKAL);
        REQUIRE( mi.common_name() == "шакал зомби" );

    }
}