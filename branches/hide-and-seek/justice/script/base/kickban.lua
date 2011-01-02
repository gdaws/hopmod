local VERY_LONG_BANTIME = 63072000
local SHORT_BANTIME = 14400

local kick_signal = server.create_event_signal("kick")
local temporary_bans = {}

local function kick_banned_players(ipmask, bantime, admin, reason)

    for _, cn in ipairs(server.clients()) do
        if net.ipmask(server.player_iplong(cn)) == net.ipmask(ipmask) then
            server.disconnect(cn, server.DISC_KICK, reason)
            kick_signal(cn, bantime, admin or "", reason or "")
        end
    end
end

function server.kick(cn, bantime, admin, reason)

    if server.player_isbot(cn) then
        server.delbot(cn)
        return
    end

    if not bantime then 
        bantime = SHORT_BANTIME
    else
        if bantime == -1 then
            bantime = VERY_LONG_BANTIME
        end
    end
    
    server.ban(server.player_iplong(cn), bantime, admin, reason)
end

function server.ban(ipmask, bantime, admin, reason)

    if not bantime or bantime == -1 then
        bantime = VERY_LONG_BANTIME
    end
    
    reason = reason or ""
    
    server.set_ip_var(ipmask, "ban_time", os.date())
    server.set_ip_var(ipmask, "ban_expire", os.time() + bantime)
    server.set_ip_var(ipmask, "ban_admin", admin)
    server.set_ip_var(ipmask, "ban_reason", reason)
    
    if bantime <= SHORT_BANTIME then
        temporary_bans[#temporary_bans] = ipmask
    end
    
    kick_banned_players(ipmask, bantime, admin, reason)
end

function server.unban(ipmask)
    
    local log_message = "Removing ban on " .. net.ipmask(ipmask):to_string()
    
    server.log(log_message)
    server.log_status(log_message)
    
    server.set_ip_var(ipmask, "ban_expire", nil)
    server.set_ip_var(ipmask, "ban_admin", nil)
    server.set_ip_var(ipmask, "ban_reason", nil)
    server.set_ip_var(ipmask, "ban_time", nil)
end

server.event_handler("connecting", function(cn, hostname, name, password)
    local bantime = server.ip_vars(hostname).ban_expire
    if bantime then
        if bantime > os.time() then
            return -1
        else
            server.unban(hostname)
        end
    end
end)

server.event_handler("clearbans_request", function()
    
    for _, iplong in pairs(temporary_bans) do
        server.unban(iplong)
    end
    
    temporary_bans = {}
    
    server.msg("cleared all bans")
end)

server.event_handler("kick_request", function(admin_cn, admin_name, bantime, target, reason)
    server.kick(target, bantime, admin_name, reason)
end)

server.event_handler("started", function()
    
    -- Don't run on server reload
    if server.uptime > 1 then return end 

    local bancount = 0
    
    -- Clear temporary/near-expiry bans
    for ipmask, vars in pairs(server.ip_vars()) do
        if vars.ban_expire then
            if vars.ban_expire - SHORT_BANTIME <= os.time() then
                server.unban(ipmask)
            else
                bancount = bancount + 1
            end
        end
    end
    
    if bancount > 0 then
        server.log_status(string.format("Ban count: %i", bancount))
    end
end)
