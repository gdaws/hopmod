#include <cassert>
#include <lua.hpp>
#include "core_bindings.hpp"
#include "events.hpp"
#include "lua/modules.hpp"

static void load_lua_modules();

static lua_State * L = NULL;
static lua::event_environment * event_environment = NULL;

void init_lua()
{
    L = luaL_newstate();
    luaL_openlibs(L);
    
    lua_newtable(L);
    int core_table = lua_gettop(L);
    
    bind_core_functions(L, core_table);
    
    lua_pushliteral(L, "vars");
    lua_newtable(L);
    int vars_table = lua_gettop(L);
    
    bind_core_constants(L, vars_table);
    bind_core_variables(L, vars_table);
    
    lua_settable(L, -3); // Add vars table to core table
    lua_setglobal(L, "core"); // Add core table to global table
    
    event_environment = new lua::event_environment(L);
    register_event_idents(*event_environment); // Setup and populate the event table
    
    load_lua_modules();
}

void shutdown_lua()
{
    delete event_environment;
    lua_close(L);
    
    event_environment = NULL;
    L = NULL;
}

lua_State * get_lua_state()
{
    return L;
}

static void load_lua_modules()
{
    lua::module::open_net(L);
    lua::module::open_timer(L);
    lua::module::open_crypto(L);
    lua::module::open_cubescript(L);
    lua::module::open_geoip(L);
    lua::module::open_filesystem(L);
    lua::module::open_http_server(L);
    lua_packlibopen(L);
#ifdef HAS_LSQLITE3
    luaopen_lsqlite3(L);
    lua_pop(L, 1);
#endif
}

lua::event_environment & event_listeners()
{
    static lua::event_environment unready_event_environment;
    if(!event_environment) return unready_event_environment;
    return *event_environment;
}

