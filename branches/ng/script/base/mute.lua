local mute_time = 1000*60*60*4
local key_function = server.player_iplong
local muted = {}
local mute_spectators = false

function server.mute(cn)

    local key = key_function(cn)
    
    muted[key] = true
    
    for i,cn in ipairs(server.players()) do 
        if key_function(cn) == key then
            server.player_msg(cn, red("You have been muted by an admin; your future chat messages will be blocked."))
        end
    end
    
    server.sleep(mute_time, function()
        muted[key] = nil
    end)
end

function server.unmute(cn)

    local key = key_function(cn)
    
    muted[key] = nil
    
    for i,cn in ipairs(server.players()) do
        server.player_msg(cn, "You have been unmuted.")
    end
end

function server.is_muted(cn)
    return muted[key_function(cn)]
end

function server.mute_spectators()
    mute_spectators = true
end

function server.unmute_spectators()
    mute_spectators = false
end

local function block_text(cn,text)

    local is_muted = muted[key_function(cn)] or (mute_spectators and server.player_status_code(cn) == server.SPECTATOR and server.player_priv_code(cn) ~= server.PRIV_ADMIN)
    
    if is_muted then
        server.player_msg(cn, red("Your chat messages are being blocked."))
        return -1
    end
end

local text_event = server.event_handler("text", block_text)
local sayteam_event = server.event_handler("sayteam", block_text)

local function unload()
    
    server.cancel_handler(text_event)
    server.cancel_handler(sayteam_event)
    
    server.mute = nil
    server.unmute = nil
end

return {unload = unload}
