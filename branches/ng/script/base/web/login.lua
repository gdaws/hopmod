require "crypto"

local sessions = {}

local function generateSessionKey()
    return crypto.sauerecc.generate_key_pair()
end

local function isLoggedIn(request)
    local params = http_request.parse_cookie(request:header("cookie"))
    local sessionId = params.id
    if not sessionId then return false end
    local sessionInfo = sessions[sessionId]
    if not sessionInfo then return false end
    if sessionInfo.ip ~= request:client_ip() then return false end
    return true
end

local function requireLogin(request)
    if request:client_ip() ~= "127.0.0.1" and not isLoggedIn(request) then
        http_response.redirect(request, "http://" .. request:host() .. "/login?return=" .. http_request.absolute_uri(request))
        return true
    end
    return false
end

local function requireBackendLogin(request)
    if request:client_ip() ~= "127.0.0.1" and not isLoggedIn(request) then
        http_response.send_error(request, 401, "You must be logged in to access this resource.\n",{["WWW-Authenticate"] = "HopmodWebLogin"})
        return true
    end
    return false
end

web_admin = {
    require_login = requireLogin,
    require_backend_login = requireBackendLogin
}

local function tryLogin(username, password)
    return username == server.web_admin_username and password == server.web_admin_password
end

local function getLoginFormHtml(attributes)
    
    local failed = ""
    local username = ""
    
    if attributes.failed then
        failed = "<p class=\"form-error\">Invalid username or password.</p>"
    end
    
    local html = [[
<html>
    <head>
        <title>Hopmod Gameserver Admin Login</title>
        <link rel="stylesheet" type="text/css" href="/static/presentation/screen.css" />
        <script type="text/javascript">
            window.onload=function(){document.forms.login.username.focus();}
        </script>
    </head>
    <body>
        <form method="post" id="login-form" name="login" action="?return=%s">
            %s
            <fieldset>
            <p><label for="username">Username</label><input type="text" name="username" value="%s" /></p>
            <p><label for="password">Password</label><input type="password" name="password" /></p>
            </fieldset>
            <p><input type="submit" value="Login" /></p>
        </form>
    </body>
</html>
    ]]
    
    return string.format(html, attributes.returnUrl or "", failed, attributes.username or "")
end

http_server.bind("login", http_server.resource({

    get = function(request)
        local uri_query_params = http_request.parse_query_string(request:uri_query() or "")
        http_response.send_html(request, getLoginFormHtml({failed=false, returnUrl = uri_query_params["return"]}))
    end,
    
    post = function(request)
        
        local referer = request:header("referer")
        
        local ctype = request:header("content-type")
        
        if ctype ~= "application/x-www-form-urlencoded" then
            http_response.send_error(request, 400, "expected application/x-www-form-urlencoded type content")
            return
        end
        
        request:async_read_content(function(content)
        
            local uri_query_params = http_request.parse_query_string(request:uri_query() or "")
            local params = http_request.parse_query_string(content)
            
            local missing_fields = not params.username or not params.password
            
            if missing_fields or tryLogin(params.username, params.password) == false then
                http_response.send_html(request, 
                    getLoginFormHtml({failed=true, 
                        returnUrl = uri_query_params["return"],
                        username = params.username
                    }))
                return
            end
            
            local sessionId = generateSessionKey()
            
            sessions[sessionId] = {
                ip = request:client_ip()
            }

            local cookie = http_request.build_query_string({id = sessionId})
            
            http_response.redirect(request, http_utils.return_url(request, uri_query_params["return"]), {["Set-Cookie"] = cookie})
        end)
    end
}))

http_server.bind("logout", http_server.resource({
    get = function(request)
        
    end
}))