Name
====

ngx_lua - Embed the power of Lua into Nginx

*This module is not distributed with the Nginx source.* See [the installation instructions](http://wiki.nginx.org/HttpLuaModule#Installation).

Status
======

This module is under active development and is already production ready.

Version
=======

This document describes ngx_lua [v0.3.1rc8](https://github.com/chaoslawful/lua-nginx-module/downloads) released on 23 September 2011.

Synopsis
========

    # set search paths for pure Lua external libraries (';;' is the default path):
    lua_package_path '/foo/bar/?.lua;/blah/?.lua;;';
 
    # set search paths for Lua external libraries written in C (can also use ';;'):
    lua_package_cpath '/bar/baz/?.so;/blah/blah/?.so;;';
 
    server {
        location /inline_concat {
            # MIME type determined by default_type:
            default_type 'text/plain';
 
            set $a "hello";
            set $b "world";
            # inline lua script
            set_by_lua $res "return ngx.arg[1]..ngx.arg[2]" $a $b;
            echo $res;
        }
 
        location /rel_file_concat {
            set $a "foo";
            set $b "bar";
            # script path relative to nginx prefix
            # $ngx_prefix/conf/concat.lua contents:
            #
            #    return ngx.arg[1]..ngx.arg[2]
            #
            set_by_lua_file $res conf/concat.lua $a $b;
            echo $res;
        }
 
        location /abs_file_concat {
            set $a "fee";
            set $b "baz";
            # absolute script path not modified
            set_by_lua_file $res /usr/nginx/conf/concat.lua $a $b;
            echo $res;
        }
 
        location /lua_content {
            # MIME type determined by default_type:
            default_type 'text/plain';
 
            content_by_lua "ngx.say('Hello,world!')"
        }
 
         location /nginx_var {
            # MIME type determined by default_type:
            default_type 'text/plain';
 
            # try access /nginx_var?a=hello,world
            content_by_lua "ngx.print(ngx.var['arg_a'], '\\n')";
        }
 
        location /request_body {
             # force reading request body (default off)
             lua_need_request_body on;
             client_max_body_size 50k;
             client_body_buffer_size 50k;
 
             content_by_lua 'ngx.print(ngx.var.request_body)';
        }
 
        # transparent non-blocking I/O in Lua via subrequests
        location /lua {
            # MIME type determined by default_type:
            default_type 'text/plain';
 
            content_by_lua '
                local res = ngx.location.capture("/some_other_location")
                if res.status == 200 then
                    ngx.print(res.body)
                end';
        }
 
        # GET /recur?num=5
        location /recur {
            # MIME type determined by default_type:
            default_type 'text/plain';
 
            content_by_lua '
               local num = tonumber(ngx.var.arg_num) or 0
               ngx.say("num is: ", num)
 
               if num > 0 then
                   res = ngx.location.capture("/recur?num=" .. tostring(num - 1))
                   ngx.print("status=", res.status, " ")
                   ngx.print("body=", res.body)
               else
                   ngx.say("end")
               end
               ';
        }
 
        location /foo {
            rewrite_by_lua '
                res = ngx.location.capture("/memc",
                    { args = { cmd = 'incr', key = ngx.var.uri } }
                )
            ';
 
            proxy_pass http://blah.blah.com;
        }
 
        location /blah {
            access_by_lua '
                local res = ngx.location.capture("/auth")
 
                if res.status == ngx.HTTP_OK then
                    return
                end
 
                if res.status == ngx.HTTP_FORBIDDEN then
                    ngx.exit(res.status)
                end
 
                ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)
            ';
 
            # proxy_pass/fastcgi_pass/postgres_pass/...
        }
 
        location /mixed {
            rewrite_by_lua_file /path/to/rewrite.lua;
            access_by_lua_file /path/to/access.lua;
            content_by_lua_file /path/to/content.lua;
        }
 
        # use nginx var in code path
        # WARN: contents in nginx var must be carefully filtered,
        # otherwise there'll be great security risk!
        location ~ ^/app/(.+) {
                content_by_lua_file /path/to/lua/app/root/$1.lua;
        }
 
        location / {
           lua_need_request_body on;
 
           client_max_body_size 100k;
           client_body_buffer_size 100k;
 
           access_by_lua '
               -- check the client IP addr is in our black list
               if ngx.var.remote_addr == "132.5.72.3" then
                   ngx.exit(ngx.HTTP_FORBIDDEN)
               end
 
               -- check if the request body contains bad words
               if ngx.var.request_body and
                        string.match(ngx.var.request_body, "fsck")
               then
                   return ngx.redirect("/terms_of_use.html")
               end
 
               -- tests passed
           ';
 
           # proxy_pass/fastcgi_pass/etc settings
        }
    }

Description
===========

This module embeds the Lua interpreter or LuaJIT into the nginx core and integrates the powerful Lua threads (aka Lua coroutines) into the nginx event model
by means of nginx subrequests.

Unlike [Apache's mod_lua](http://httpd.apache.org/docs/2.3/mod/mod_lua.html) and [Lighttpd's mod_magnet](http://redmine.lighttpd.net/wiki/1/Docs:ModMagnet), Lua code written atop this module can be *100% non-blocking* on network traffic
as long as you use the [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) or
[ngx.location.capture_multi](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture_multi) interfaces
to let the nginx core do all your
requests to mysql, postgresql, memcached, redis,
upstream http web services, and etc etc etc (see
[HttpDrizzleModule](http://wiki.nginx.org/HttpDrizzleModule), [ngx_postgres](http://github.com/FRiCKLE/ngx_postgres/), [HttpMemcModule](http://wiki.nginx.org/HttpMemcModule), [HttpRedis2Module](http://wiki.nginx.org/HttpRedis2Module) and [HttpProxyModule](http://wiki.nginx.org/HttpProxyModule) modules for details).

The Lua interpreter instance is shared across all
the requests in a single nginx worker process.

Request contexts are isolated from each other
by means of Lua (lightweight) threads (aka Lua coroutines).
And Lua modules loaded are persistent on
the nginx worker process level. So the memory
footprint is quite small even when your
nginx worker process is handling 10K requests at the same time.

Directives
==========

lua_code_cache
--------------
**syntax:** *lua_code_cache on | off*

**default:** *lua_code_cache on*

**context:** *main, server, location, location if*

Enable or disable the Lua code cache for [set_by_lua_file](http://wiki.nginx.org/HttpLuaModule#set_by_lua_file),
[content_by_lua_file](http://wiki.nginx.org/HttpLuaModule#content_by_lua_file), [rewrite_by_lua_file](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua_file), and
[access_by_lua_file](http://wiki.nginx.org/HttpLuaModule#access_by_lua_file), and also force Lua module reloading on a per-request basis.

The Lua files referenced in [set_by_lua_file](http://wiki.nginx.org/HttpLuaModule#set_by_lua_file),
[content_by_lua_file](http://wiki.nginx.org/HttpLuaModule#content_by_lua_file), [access_by_lua_file](http://wiki.nginx.org/HttpLuaModule#access_by_lua_file),
and [rewrite_by_lua_file](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua_file) won't be cached at all,
and Lua's `package.loaded` table will be cleared
at every request's entry point (such that Lua modules
won't be cached either). So developers and enjoy
the PHP-way, i.e., edit-and-refresh.

But please note that Lua code inlined into nginx.conf
like those specified by [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua), [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua),
[access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua), and [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) will *always* be
cached because only nginx knows how to parse `nginx.conf`
and the only way to tell it to re-load the config file
is to send a `HUP` signal to it or just to restart it from scratch.

For now, ngx_lua does not support the "stat" mode like
Apache's `mod_lua`, but we will work on it in the future.

Disabling the Lua code cache is mainly used for Lua
development only because it has great
impact on the over-all performance and is strongly
discouraged for production uses. Also, race conditions
when reloading Lua modules are common for concurrent requests
when the code cache is off.

lua_regex_cache_max_entries
---------------------------
**syntax:** *lua_regex_cache_max_entries &lt;num&gt;*

**default:** *lua_regex_cache_max_entries 1024*

**context:** *http*

Specifies the maximal entries allowed in the worker-process-level compiled-regex cache.

The regular expressions used in [ngx.re.match](http://wiki.nginx.org/HttpLuaModule#ngx.re.match), [ngx.re.gmatch](http://wiki.nginx.org/HttpLuaModule#ngx.re.gmatch), [ngx.re.sub](http://wiki.nginx.org/HttpLuaModule#ngx.re.sub), and [ngx.re.gsub](http://wiki.nginx.org/HttpLuaModule#ngx.re.gsub) will be cached in this cache if the regex option `o` (i.e., compile-once flag) is specified.

The default entries allowed is 1024.

When the user Lua programs are exceeding this limit, those new regexes will not be cached at all (as if no `o` option is ever specified), and there will be one (and only one) warning in nginx's `error.log` file, like this

    2011/08/27 23:18:26 [warn] 31997#0: *1 lua exceeding regex cache max entries (1024), ...


You shouldn't specify the `o` regex option for regexes (and/or `replace` string arguments for [ngx.re.sub](http://wiki.nginx.org/HttpLuaModule#ngx.re.sub) and [ngx.re.gsub](http://wiki.nginx.org/HttpLuaModule#ngx.re.gsub)) that are generated *on the fly* and give rise to infinite variations, or you'll quickly reach the limit specified here.

lua_package_path
----------------

**syntax:** *lua_package_path &lt;lua-style-path-str&gt;*

**default:** *The content of LUA_PATH environ variable or Lua's compiled-in defaults.*

**context:** *main*

Set the Lua module searching path used by scripts specified by [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua),
[content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) and others. The path string is in standard Lua path form, and `;;`
can be used to stand for the original path.

lua_package_cpath
-----------------

**syntax:** *lua_package_cpath &lt;lua-style-cpath-str&gt;*

**default:** *The content of LUA_CPATH environ variable or Lua's compiled-in defaults.*

**context:** *main*

Set the Lua C-module searching path used by scripts specified by [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua),
[content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) and others. The cpath string is in standard Lua cpath form, and `;;`
can be used to stand for the original cpath.

set_by_lua
----------

**syntax:** *set_by_lua $res &lt;lua-script-str&gt; [$arg1 $arg2 ...]*

**context:** *main, server, location, server if, location if*

**phase:** *rewrite*

Execute user code specified by `<lua-script-str>` with input arguments `$arg1 $arg2 ...`, and set the script's return value to `$res` in string form. In
`<lua-script-str>` code the input arguments can be retrieved from `ngx.arg`
table (index starts from `1` and increased sequentially).

[set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua) directives are designed to execute small and quick codes. Nginx
event loop is blocked during the code execution, so you'd better **not** call
anything that may be blocked or time-consuming.

Note that [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua) can only output a value to a single Nginx variable at
a time. But a work-around is also available by means of the [ngx.var.VARIABLE](http://wiki.nginx.org/HttpLuaModule#ngx.var.VARIABLE) interface,
for example,

    location /foo {
        set $diff ''; # we have to predefine the $diff variable here
 
        set_by_lua $sum '
            local a = 32
            local b = 56
 
            ngx.var.diff = a - b;  -- write to $diff directly
            return a + b;          -- return the $sum value normally
        ';
 
        echo "sum = $sum, diff = $diff";
    }


This directive can be freely mixed with all the directives of [HttpRewriteModule](http://wiki.nginx.org/HttpRewriteModule), [HttpSetMiscModule](http://wiki.nginx.org/HttpSetMiscModule), and [HttpArrayVarModule](http://wiki.nginx.org/HttpArrayVarModule). All of these directives will run in exactly the same order that they are written in the config file. For example,

    set $foo 32;
    set_by_lua $bar 'tonumber(ngx.var.foo) + 1';
    set $baz "bar: $bar";  # $baz == "bar: 33"


This directive requires the [ngx_devel_kit](https://github.com/simpl/ngx_devel_kit) module.

set_by_lua_file
---------------

**syntax:** *set_by_lua_file $res &lt;path-to-lua-script&gt; [$arg1 $arg2 ...]*

**context:** *main, server, location, server if, location if*

Basically the same as [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua), except the code to be executed is in the
file specified by `<path-lua-script>`.

When the Lua code cache is on (this is the default), the user code is loaded
once at the first request and cached. Nginx config must be reloaded if you
modified the file and expected to see updated behavior. You can disable the
Lua code cache by setting `lua_code_cache off;` in your nginx.conf.

This directive requires the [ngx_devel_kit](https://github.com/simpl/ngx_devel_kit) module.

content_by_lua
--------------

**syntax:** *content_by_lua &lt;lua-script-str&gt;*

**context:** *location, location if*

**phase:** *content*

Act as a content handler and execute user code specified by `<lua-script-str>`
for every request. The user code may call predefined APIs to generate response
content.

The use code is executed in a new spawned coroutine with independent global environment (i.e. a sandbox).

Do not use this directive and other content handler directives in a same location. For example, it's bad to use this directive with a [proxy_pass](http://wiki.nginx.org/HttpProxyModule#proxy_pass) directive in the same location.

content_by_lua_file
-------------------

**syntax:** *content_by_lua_file &lt;path-to-lua-script&gt;*

**context:** *location, location if*

**phase:** *content*

Basically the same as [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua), except the code to be executed is in
the file specified by `<path-lua-script>`.

Nginx variables can be used in `<path-to-lua-script>` string, in order to provide
greater flexibility in practice. But this feature must be used carefully, so is
not recommend for beginners.

When the Lua code cache is on (this is the default), the user code is loaded once at the first request and cached. Nginx config must be reloaded if you modified the file and expected to see updated behavior. You can disable the Lua code cache by setting [lua_code_cache](http://wiki.nginx.org/HttpLuaModule#lua_code_cache) `off` in your `nginx.conf` file.

rewrite_by_lua
--------------

**syntax:** *rewrite_by_lua &lt;lua-script-str&gt;*

**context:** *http, server, location, location if*

**phase:** *rewrite tail*

Act as a rewrite phase handler and execute user code specified by `<lua-script-str>`
for every request. The user code may call predefined APIs to generate response
content.

This hook uses exactly the same mechamism as [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) so all the nginx APIs defined there
are also available here.

Note that this handler always runs *after* the standard [HttpRewriteModule](http://wiki.nginx.org/HttpRewriteModule). So the following will work as expected:


       location /foo {
           set $a 12; # create and initialize $a
           set $b ""; # create and initialize $b
           rewrite_by_lua 'ngx.var.b = tonumber(ngx.var.a) + 1';
           echo "res = $b";
       }


because `set $a 12` and `set $b ""` run *before* [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua).

On the other hand, the following will not work as expected:


    ?  location /foo {
    ?      set $a 12; # create and initialize $a
    ?      set $b ''; # create and initialize $b
    ?      rewrite_by_lua 'ngx.var.b = tonumber(ngx.var.a) + 1';
    ?      if ($b = '13') {
    ?         rewrite ^ /bar redirect;
    ?         break;
    ?      }
    ?
    ?      echo "res = $b";
    ?  }


because `if` runs *before* [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) even if it's put after [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) in the config.

The right way of doing this is as follows:


    location /foo {
        set $a 12; # create and initialize $a
        set $b ''; # create and initialize $b
        rewrite_by_lua '
            ngx.var.b = tonumber(ngx.var.a) + 1
            if ngx.var.b == 13 then
                return ngx.redirect("/bar");
            end
        ';
 
        echo "res = $b";
    }


It's worth mentioning that, the `ngx_eval` module can be approximately implemented by [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua). For example,


    location / {
        eval $res {
            proxy_pass http://foo.com/check-spam;
        }
 
        if ($res = 'spam') {
            rewrite ^ /terms-of-use.html redirect;
        }
 
        fastcgi_pass ...;
    }


can be implemented in terms of `ngx_lua` like this


    location = /check-spam {
        internal;
        proxy_pass http://foo.com/check-spam;
    }
 
    location / {
        rewrite_by_lua '
            local res = ngx.location.capture("/check-spam")
            if res.body == "spam" then
                ngx.redirect("/terms-of-use.html")
            end
        ';
 
        fastcgi_pass ...;
    }


Just as any other rewrite phase handlers, [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) also runs in subrequests.

Note that calling `ngx.exit(ngx.OK)` just returning from the current [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) handler, and the nginx request processing control flow will still continue to the content handler. To terminate the current request from within the current [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) handler, calling [ngx.exit](http://wiki.nginx.org/HttpLuaModule#ngx.exit) with status >= 200 (`ngx.HTTP_OK`) and status < 300 (`ngx.HTTP_SPECIAL_RESPONSE`) for successful quits and `ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)` (or its friends) for failures.

rewrite_by_lua_file
-------------------

**syntax:** *rewrite_by_lua_file &lt;path-to-lua-script&gt;*

**context:** *http, server, location, location if*

**phase:** *rewrite tail*

Same as [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua), except the code to be executed is in
the file specified by `<path-lua-script>`.

Nginx variables can be used in `<path-to-lua-script>` string, in order to provide
greater flexibility in practice. But this feature must be used carefully, so is
not recommend for beginners.

When the Lua code cache is on (this is the default), the user code is loaded
once at the first request and cached. Nginx config must be reloaded if you
modified the file and expected to see updated behavior. You can disable the
Lua code cache by setting [lua_code_cache](http://wiki.nginx.org/HttpLuaModule#lua_code_cache) `off` in your `nginx.conf` file.

access_by_lua
-------------

**syntax:** *access_by_lua &lt;lua-script-str&gt;*

**context:** *http, server, location, location if*

**phase:** *access tail*

Act as an access phase handler and execute user code specified by `<lua-script-str>` for every request. The user code may call predefined APIs to generate response content.

This hook uses exactly the same mechanism as [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) so all the nginx APIs defined there are also available here.

Note that this handler always runs *after* the standard [HttpAccessModule](http://wiki.nginx.org/HttpAccessModule). So the following will work as expected:


    location / {
        deny    192.168.1.1;
        allow   192.168.1.0/24;
        allow   10.1.1.0/16;
        deny    all;
 
        access_by_lua '
            local res = ngx.location.capture("/mysql", { ... })
            ...
        ';
 
        # proxy_pass/fastcgi_pass/...
    }


That is, if a client address appears in the blacklist, then we don't have to bother sending a MySQL query to do more advanced authentication in [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua).

It's worth mentioning that, the `ngx_auth_request` module can be approximately implemented by [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua). For example,


    location / {
        auth_request /auth;
 
        # proxy_pass/fastcgi_pass/postgres_pass/...
    }


can be implemented in terms of `ngx_lua` like this


    location / {
        access_by_lua '
            local res = ngx.location.capture("/auth")
 
            if res.status == ngx.HTTP_OK then
                return
            end
 
            if res.status == ngx.HTTP_FORBIDDEN then
                ngx.exit(res.status)
            end
 
            ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)
        ';
 
        # proxy_pass/fastcgi_pass/postgres_pass/...
    }


Just as any other access phase handlers, [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua) will *not* run in subrequests.

Note that calling `ngx.exit(ngx.OK)` just returning from the current [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua) handler, and the nginx request processing control flow will still continue to the content handler. To terminate the current request from within the current [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua) handler, calling `ngx.exit(status)` where status >= 200 (`ngx.HTTP_OK`) and status < 300 (`ngx.HTTP_SPECIAL_RESPONSE`) for successful quits and `ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)` or its friends for failures.

access_by_lua_file
------------------

**syntax:** *access_by_lua_file &lt;path-to-lua-script&gt;*

**context:** *http, server, location, location if*

**phase:** *access tail*

Same as [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua), except the code to be executed is in the file
specified by `<path-lua-script>`.

Nginx variables can be used in `<path-to-lua-script>` string, in order to provide
greater flexibility in practice. But this feature must be used carefully, so is
not recommend for beginners.

When the Lua code cache is on (this is the default), the user code is loaded
once at the first request and cached. Nginx config must be reloaded if you
modified the file and expected to see updated behavior. You can disable the
Lua code cache by setting [lua_code_cache](http://wiki.nginx.org/HttpLuaModule#lua_code_cache) `off` in your `nginx.conf` file.

header_filter_by_lua
--------------------

**syntax:** *header_filter_by_lua &lt;lua-script-str&gt;*

**context:** *http, server, location, location if*

**phase:** *output header filter*

Use Lua defined in `<lua-script-str>` to define an output header filter. For now, the following Nginx Lua APIs are disabled in this context:

* Output API (e.g., [ngx.say](http://wiki.nginx.org/HttpLuaModule#ngx.say) and [ngx.send_headers](http://wiki.nginx.org/HttpLuaModule#ngx.send_headers))
* Control APIs (e.g., [ngx.exit](http://wiki.nginx.org/HttpLuaModule#ngx.exit)) 
* Subrequest APIs (e.g., [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) and [ngx.location.capture_multi](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture_multi))

Here's a small example of overriding a response header (or adding if it does not exist) in our Lua header filter:

    location / {
        proxy_pass http://mybackend;
        header_filter_by_lua 'ngx.header.Foo = "blah"';
    }


This directive was first introduced in the `v0.2.1rc20` release.

header_filter_by_lua_file
-------------------------

**syntax:** *header_filter_by_lua_file &lt;path-to-lua-script-file&gt;*

**context:** *http, server, location, location if*

**phase:** *output header filter*

Use Lua code defined in a separate file specified by `<path-to-lua-script-file>` to define an output header filter.

This is very much like [header_filter_by_lua](http://wiki.nginx.org/HttpLuaModule#header_filter_by_lua) except that it loads Lua code from an external Lua source file.

This directive was first introduced in the `v0.2.1rc20` release.

lua_need_request_body
---------------------

**syntax:** *lua_need_request_body &lt;on | off&gt;*

**default:** *off*

**context:** *main | server | location*

**phase:** *depends on usage*

Force reading request body data or not. The client request body won't be read, so you have to explicitly force reading the body if you need its content.

If you want to read the request body data from the [$request_body](http://wiki.nginx.org/HttpCoreModule#.24request_body) variable, make sure that
your have configured [client_body_buffer_size](http://wiki.nginx.org/HttpCoreModule#client_body_buffer_size) to have exactly the same value as [client_max_body_size](http://wiki.nginx.org/HttpCoreModule#client_max_body_size).

If the current location defines [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) or [rewrite_by_lua_file](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua_file),
then the request body will be read just before the [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) or [rewrite_by_lua_file](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua_file) code is run (and also at the
`rewrite` phase). Similarly, if only [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) is specified,
the request body won't be read until the content handler's Lua code is
about to run (i.e., the request body will be read at the
content phase).

The same applies to [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua) and [access_by_lua_file](http://wiki.nginx.org/HttpLuaModule#access_by_lua_file).

Nginx API for Lua
=================

The Nginx API exposed to the Lua land is provided in the form of two standard packages `ngx` and `ndk`. These packages are in the default global scope.

When you're writing your own external Lua modules, however, you can introduce these packages by using the [package.seeall](http://www.lua.org/manual/5.1/manual.html#pdf-package.seeall) option:


    module("my_module", package.seeall)

    function say(a) ngx.say(a) end


Alternatively, import them to your Lua modules by using file-scoped local Lua variables, like this:


    local ngx = ngx
    module("my_module")

    function say(a) ngx.say(a) end


You can directly require the standard packages `ngx` and `ndk` introduced by this Nginx module, like this:


    local ngx = require "ngx"
    local ndk = require "ndk"


The ability to require these packages was introduced in the `v0.2.1rc19` release.

Network I/O operations in user code should only be done through our Nginx APIs defined below, otherwise Nginx event loop may be blocked and performance may drop off dramatically. Small disk file operations can be done via Lua's standard `io` and `file` libraries but should be eliminated wherever possible because these also block the Nginx process. Delegating all network and disk I/O operations to Nginx subrequests (via the [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.catpure) method and its friends) are strongly recommended.

ngx.arg
-------
**syntax:** *val = ngx.arg[index]*

**context:** *set_by_lua**

Index the input arguments to the [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua) and [set_by_lua_file](http://wiki.nginx.org/HttpLuaModule#set_by_lua_file) directives:


    value = ngx.arg[n]


Here's an example


    location /foo {
        set $a 32;
        set $b 56;
 
        set_by_lua $res
            'return tonumber(ngx.arg[1]) + tonumber(ngx.arg[2])'
            $a $b;
 
        echo $sum;
    }


that outputs `88`, the sum of `32` and `56`.

ngx.var.VARIABLE
----------------
**syntax:** *ngx.var.VAR_NAME*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

    value = ngx.var.some_nginx_variable_name
    ngx.var.some_nginx_variable_name = value

Note that you can only write to nginx variables that are already defined.
For example:

    location /foo {
        set $my_var ''; # this line is required to create $my_var at config time
        content_by_lua '
            ngx.var.my_var = 123;
            ...
        ';
    }

That is, nginx variables cannot be created on-the-fly.

Some special nginx variables like `$args` and `$limit_rate` can be assigned a value,
some are not, like `$arg_PARAMETER`.

Nginx regex group capturing variables `$1`, `$2`, `$3`, and etc, can be read by this
interface as well, by writing `ngx.var[1]`, `ngx.var[2]`, `ngx.var[3]`, and etc.

Core constants
--------------
**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**


      ngx.OK (0)
      ngx.ERROR (-1)
      ngx.AGAIN (-2)
      ngx.DONE (-4)

They take the same values of `NGX_OK`, `NGX_AGAIN`, `NGX_DONE`, `NGX_ERROR`, and etc. But now
only [ngx.exit](http://wiki.nginx.org/HttpLuaModule#ngx.exit) only take two of these values, i.e., `NGX_OK` and `NGX_ERROR`.

HTTP method constants
---------------------
**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**


      ngx.HTTP_GET
      ngx.HTTP_HEAD
      ngx.HTTP_PUT
      ngx.HTTP_POST
      ngx.HTTP_DELETE


These constants are usually used in [ngx.location.catpure](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) and [ngx.location.capture_multi](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture_multi) method calls.

HTTP status constants
---------------------
**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

      value = ngx.HTTP_OK (200)
      value = ngx.HTTP_CREATED (201)
      value = ngx.HTTP_SPECIAL_RESPONSE (300)
      value = ngx.HTTP_MOVED_PERMANENTLY (301)
      value = ngx.HTTP_MOVED_TEMPORARILY (302)
      value = ngx.HTTP_SEE_OTHER (303)
      value = ngx.HTTP_NOT_MODIFIED (304)
      value = ngx.HTTP_BAD_REQUEST (400)
      value = ngx.HTTP_UNAUTHORIZED (401)
      value = ngx.HTTP_FORBIDDEN (403)
      value = ngx.HTTP_NOT_FOUND (404)
      value = ngx.HTTP_NOT_ALLOWED (405)
      value = ngx.HTTP_GONE (410)
      value = ngx.HTTP_INTERNAL_SERVER_ERROR (500)
      value = ngx.HTTP_SERVICE_UNAVAILABLE (503)


Nginx log level constants
-------------------------
**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

      ngx.STDERR
      ngx.EMERG
      ngx.ALERT
      ngx.CRIT
      ngx.ERR
      ngx.WARN
      ngx.NOTICE
      ngx.INFO
      ngx.DEBUG


These constants are usually used by the [ngx.log](http://wiki.nginx.org/HttpLuaModule#ngx.log) method.

print
-----
**syntax:** *print(...)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Emit args concatenated to nginx's `error.log` file, with log level `ngx.NOTICE` and prefix `lua print: `.

It's equivalent to

    ngx.log(ngx.NOTICE, 'lua print: ', a, b, ...)

Lua `nil` arguments are accepted and result in literal `"nil"`, and Lua booleans result in `"true"` or `"false"`.

ngx.ctx
-------
**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

This table can be used to store per-request context data for Lua programmers.

This table has a liftime identical to the current request (just like Nginx variables). Consider the following example,

    location /test {
        rewrite_by_lua '
            ngx.say("foo = ", ngx.ctx.foo)
            ngx.ctx.foo = 76
        ';
        access_by_lua '
            ngx.ctx.foo = ngx.ctx.foo + 3
        ';
        content_by_lua '
            ngx.say(ngx.ctx.foo)
        ';
    }

Then `GET /test` will yield the output

    foo = nil
    79

That is, the `ngx.ctx.foo` entry persists across the rewrite, access, and content phases of a request.

Also, every request has its own copy, include subrequests, for example:

    location /sub {
        content_by_lua '
            ngx.say("sub pre: ", ngx.ctx.blah)
            ngx.ctx.blah = 32
            ngx.say("sub post: ", ngx.ctx.blah)
        ';
    }
 
    location /main {
        content_by_lua '
            ngx.ctx.blah = 73
            ngx.say("main pre: ", ngx.ctx.blah)
            local res = ngx.location.capture("/sub")
            ngx.print(res.body)
            ngx.say("main post: ", ngx.ctx.blah)
        ';
    }

Then `GET /main` will give the output

    main pre: 73
    sub pre: nil
    sub post: 32
    main post: 73

We can see that modification of the `ngx.ctx.blah` entry in the subrequest does not affect the one in its parent request. They do have two separate versions of `ngx.ctx.blah` per se.

Internal redirection will destroy the original request's `ngx.ctx` data (if any) and the new request will have an emptied `ngx.ctx` table. For instance,

    location /new {
        content_by_lua '
            ngx.say(ngx.ctx.foo)
        ';
    }
 
    location /orig {
        content_by_lua '
            ngx.ctx.foo = "hello"
            ngx.exec("/new")
        ';
    }

Then `GET /orig` will give you

    nil

rather than the original `"hello"` value.

Arbitrary data values can be inserted into this "matic" table, including Lua closures and nested tables. You can also register your own meta methods with it.

Overriding `ngx.ctx` with a new Lua table is also supported, for example,

    ngx.ctx = { foo = 32, bar = 54 }


ngx.location.capture
--------------------
**syntax:** *res = ngx.location.capture(uri, options?)*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Issue a synchronous but still non-blocking *Nginx Subrequest* using `uri`.

Nginx subrequests provide a powerful way to make non-blocking internal requests to other locations configured with disk file directory or *any* other nginx C modules like `ngx_proxy`, `ngx_fastcgi`, `ngx_memc`,
`ngx_postgres`, `ngx_drizzle`, and even `ngx_lua` itself and etc etc etc.

Also note that subrequests just mimic the HTTP interface but there's *no* extra HTTP/TCP traffic *nor* IPC involved. Everything works internally, efficiently, on the C level.

Subrequests are completely different from HTTP 301/302 redirection (via [ngx.redirect](http://wiki.nginx.org/HttpLuaModule#ngx.redirect)) and internal redirection (via [ngx.exec](http://wiki.nginx.org/HttpLuaModule#ngx.exec)).

Here's a basic example:

    res = ngx.location.capture(uri)

Returns a Lua table with three slots (`res.status`, `res.header`, and `res.body`).

`res.header` holds all the response headers of the
subrequest and it is a normal Lua table. For multi-value response headers,
the value is a Lua (array) table that holds all the values in the order that
they appear. For instance, if the subrequest response headers contains the following
lines:

    Set-Cookie: a=3
    Set-Cookie: foo=bar
    Set-Cookie: baz=blah

Then `res.header["Set-Cookie"]` will be evaluted to the table value
`{"a=3", "foo=bar", "baz=blah"}`.

URI query strings can be concatenated to URI itself, for instance,

    res = ngx.location.capture('/foo/bar?a=3&b=4')

Named locations like `@foo` are not allowed due to a limitation in
the nginx core. Use normal locations combined with the `internal` directive to
prepare internal-only locations.

An optional option table can be fed as the second
argument, which support various options like
`method`, `body`, `args`, and `share_all_vars`.
Issuing a POST subrequest, for example,
can be done as follows

    res = ngx.location.capture(
        '/foo/bar',
        { method = ngx.HTTP_POST, body = 'hello, world' }
    )

See HTTP method constants methods other than POST.
The `method` option is `ngx.HTTP_GET` by default.

The `share_all_vars` option can control whether to share nginx variables
among the current request and the new subrequest. If this option is set to `true`, then
the subrequest can see all the variable values of the current request while the current
requeset can also see any variable value changes made by the subrequest.
Note that variable sharing can have unexpected side-effects
and lead to confusing issues, use it with special
care. So, by default, the option is set to `false`.

The `args` option can specify extra url arguments, for instance,

    ngx.location.capture('/foo?a=1',
        { args = { b = 3, c = ':' } }
    )

is equivalent to

    ngx.location.capture('/foo?a=1&b=3&c=%3a')

that is, this method will automatically escape argument keys and values according to URI rules and
concatenating them together into a complete query string. Because it's all done in hand-written C,
it should be faster than your own Lua code.

The `args` option can also take plain query string:

    ngx.location.capture('/foo?a=1',
        { args = 'b=3&c=%3a' } }
    )

This is functionally identical to the previous examples.

Note that, by default, subrequests issued by [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) inherit all the
request headers of the current request. This may have unexpected side-effects on the
subrequest responses. For example, when you're using the standard `ngx_proxy` module to serve
your subrequests, then an "Accept-Encoding: gzip" header in your main request may result
in gzip'd responses that your Lua code is not able to handle properly. So always set
[proxy_pass_request_headers](http://wiki.nginx.org/HttpProxyModule#proxy_pass_request_headers) `off` in your subrequest location to ignore the original request headers.

ngx.location.capture_multi
--------------------------
**syntax:** *res1, res2, ... = ngx.location.capture_multi({ {uri, options?}, {uri, options?}, ... })*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Just like [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture), but supports multiple subrequests running in parallel.

This function issue several parallel subrequests specified by the input table, and returns their results in the same order. For example,

    res1, res2, res3 = ngx.location.capture_multi{
        { "/foo", { args = "a=3&b=4" } },
        { "/bar" },
        { "/baz", { method = ngx.HTTP_POST, body = "hello" } },
    }
 
    if res1.status == ngx.HTTP_OK then
        ...
    end
 
    if res2.body == "BLAH" then
        ...
    end

This function will not return until all the subrequests terminate.
The total latency is the longest latency of the subrequests, instead of their sum.

When you don't know inadvance how many subrequests you want to issue,
you can use Lua tables for both requests and responses. For instance,

    -- construct the requests table
    local reqs = {}
    table.insert(reqs, { "/mysql" })
    table.insert(reqs, { "/postgres" })
    table.insert(reqs, { "/redis" })
    table.insert(reqs, { "/memcached" })
 
    -- issue all the requests at once and wait until they all return
    local resps = { ngx.location.capture_multi(reqs) }
 
    -- loop over the responses table
    for i, resp in ipairs(resps) do
        -- process the response table "resp"
    end

The [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) function is just a special form
of this function. Logically speaking, the [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) can be implemented like this

    ngx.location.capture =
        function (uri, args)
            return ngx.location.capture_multi({ {uri, args} })
        end


ngx.status
----------
**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Read and write the current request's response status. This should be called
before sending out the response headers.

    ngx.status = ngx.HTTP_CREATED
    status = ngx.status


ngx.header.HEADER
-----------------
**syntax:** *ngx.header.HEADER = VALUE*

**syntax:** *value = ngx.header.HEADER*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

When assigning to `ngx.header.HEADER` will set, add, or clear the current request's response header named `HEADER`. Underscores (`_`) in the header names will be replaced by dashes (`-`) and the header names will be matched case-insensitively.


    -- equivalent to ngx.header["Content-Type"] = 'text/plain'
    ngx.header.content_type = 'text/plain';
 
    ngx.header["X-My-Header"] = 'blah blah';


Multi-value headers can be set this way:


    ngx.header['Set-Cookie'] = {'a=32; path=/', 'b=4; path=/'}


will yield


    Set-Cookie: a=32; path=/
    Set-Cookie: b=4; path=/


in the response headers. Only array-like tables are accepted.

Note that, for those standard headers that only accepts a single value, like `Content-Type`, only the last element
in the (array) table will take effect. So


    ngx.header.content_type = {'a', 'b'}


is equivalent to


    ngx.header.content_type = 'b'


Setting a slot to `nil` effectively removes it from the response headers:


    ngx.header["X-My-Header"] = nil;


same does assigning an empty table:


    ngx.header["X-My-Header"] = {};


Setting `ngx.header.HEADER` after sending out response headers (either explicitly with [ngx.send_headers](http://wiki.nginx.org/HttpLuaModule#ngx.send_headers) or implicitly with [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print) and its friends) will throw out a Lua exception.

Reading `ngx.header.HEADER` will return the value of the response header named `HEADER`. Underscores (`_`) in the header names will also be replaced by dashes (`-`) and the header names will be matched case-insensitively. If the response header is not present at all, `nil` will be returned.

This is particularly useful in the context of [filter_header_by_lua](http://wiki.nginx.org/HttpLuaModule#filter_header_by_lua) and [filter_header_by_lua_file](http://wiki.nginx.org/HttpLuaModule#filter_header_by_lua_file), for example,


    location /test {
        set $footer '';

        proxy_pass http://some-backend;

        header_filter_by_lua '
            if ngx.header["X-My-Header"] == "blah" then
                ngx.var.footer = "some value"
            end
        ';

        echo_after_body $footer;
    }


For multi-value headers, all of the values of header will be collected in order and returned as a Lua table. For example, response headers


    Foo: bar
    Foo: baz


will result in


    {"bar", "baz"}


to be returned when reading `ngx.header.Foo`.

Note that `ngx.header` is not a normal Lua table so you cannot iterate through it using Lua's `ipairs` function.

For reading *request* headers, use the [ngx.req.get_headers](http://wiki.nginx.org/HttpLuaModule#ngx.req.get_headers) function instead.

ngx.req.get_uri_args
--------------------
**syntax:** *args = ngx.req.get_uri_args()*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns a Lua table holds all of the current request's request URL query arguments.

Here's an example,

    location = /test {
        content_by_lua '
            local args = ngx.req.get_uri_args()
            for key, val in pairs(args) do
                if type(val) == "table" then
                    ngx.say(key, ": ", table.concat(val, ", "))
                else
                    ngx.say(key, ": ", val)
                end
            end
        ';
    }

Then `GET /test?foo=bar&bar=baz&bar=blah` will yield the response body

    foo: bar
    bar: baz, blah

Multiple occurrences of an argument key will result in a table value holding all of the values for that key in order.

Keys and values will be automatically unescaped according to URI escaping rules. For example, in the above settings, `GET /test?a%20b=1%61+2` will yield the output

    a b: 1a 2

Arguments without the `=<value>` parts are treated as boolean arguments. For example, `GET /test?foo&bar` will yield the outputs

    foo: true
    bar: true

That is, they will take Lua boolean values `true`. However, they're different from arguments taking empty string values. For example, `GET /test?foo=&bar=` will give something like

    foo: 
    bar: 

Empty key arguments are discarded, for instance, `GET /test?=hello&=world` will yield empty outputs.

Updating query arguments via the nginx variable `$args` (or `ngx.var.args` in Lua) at runtime are also supported:

    ngx.var.args = "a=3&b=42"
    local args = ngx.req.get_uri_args()

Here the `args` table will always look like

    {a = 3, b = 42}

regardless of the actual request query string.

ngx.req.get_post_args
---------------------
**syntax:** *ngx.req.get_post_args()*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns a Lua table holds all of the current request's POST query arguments. It's required to turn on the [lua_need_request_body](http://wiki.nginx.org/HttpLuaModule#lua_need_request_body) directive, or a Lua exception will be thrown.

Here's an example,

    location = /test {
        lua_need_request_body on;
        content_by_lua '
            local args = ngx.req.get_post_args()
            for key, val in pairs(args) do
                if type(val) == "table" then
                    ngx.say(key, ": ", table.concat(val, ", "))
                else
                    ngx.say(key, ": ", val)
                end
            end
        ';
    }

Then

    # Post request with the body 'foo=bar&bar=baz&bar=blah'
    $ curl --data 'foo=bar&bar=baz&bar=blah' localhost/test

will yield the response body like

    foo: bar
    bar: baz, blah

Multiple occurrences of an argument key will result in a table value holding all of the values for that key in order.

Keys and values will be automatically unescaped according to URI escaping rules. For example, in the above settings,

    # POST request with body 'a%20b=1%61+2'
    $ curl -d 'a%20b=1%61+2' localhost/test

will yield the output

    a b: 1a 2

Arguments without the `=<value>` parts are treated as boolean arguments. For example, `GET /test?foo&bar` will yield the outputs

    foo: true
    bar: true

That is, they will take Lua boolean values `true`. However, they're different from arguments taking empty string values. For example, `POST /test` with request body `foo=&bar=` will give something like

    foo: 
    bar: 

Empty key arguments are discarded, for instance, `POST /test` with body `=hello&=world` will yield empty outputs.

ngx.req.get_headers
-------------------
**syntax:** *headers = ngx.req.get_headers()*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns a Lua table holds all of the current request's request headers.

Here's an example,

    local h = ngx.req.get_headers()
    for k, v in pairs(h) do
        ...
    end

To read an individual header:

    ngx.say("Host: ", ngx.req.get_headers()["Host"])

For multiple instances of request headers like

    Foo: foo
    Foo: bar
    Foo: baz

the value of `ngx.req.get_headers()["Foo"]` will be a Lua (array) table like this:

    {"foo", "bar", "baz"}

Another way to read individual request headers is to use `ngx.var.http_HEADER`, that is, nginx's standard [$http_HEADER](http://wiki.nginx.org/HttpCoreModule#.24http_HEADER) variables.

ngx.req.set_header
------------------
**syntax:** *ngx.req.set_header(header_name, header_value)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Set the current request's request header named `header_name` to value `header_value`, overriding any existing ones.
None of the current request's subrequests will be affected.

Here's an example of setting the `Content-Length` header:

    ngx.req.set_header("Content-Type", "text/css")

The `header_value` can take an array list of values,
for example,

    ngx.req.set_header("Foo", {"a", "abc"})

will produce two new request headers:

    Foo: a
    Foo: abc

and old `Foo` headers will be overridden if there's any.

When the `header_value` argument is `nil`, the request header will be removed. So

    ngx.req.set_header("X-Foo", nil)

is equivalent to

    ngx.req.clear_header("X-Foo")


ngx.req.clear_header
--------------------
**syntax:** *ngx.req.clear_header(header_name)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Clear the current request's request header named `header_name`. None of the current request's subrequests will be affected.

ngx.exec
--------
**syntax:** *ngx.exec(uri, args?)*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Does an internal redirect to `uri` with `args`.


    ngx.exec('/some-location');
    ngx.exec('/some-location', 'a=3&b=5&c=6');
    ngx.exec('/some-location?a=3&b=5', 'c=6');


Named locations are also supported, but query strings are ignored. For example,


    location /foo {
        content_by_lua '
            ngx.exec("@bar");
        ';
    }
 
    location @bar {
        ...
    }


The optional second `args` can be used to specify extra URI query arguments, for example:


    ngx.exec("/foo", "a=3&b=hello%20world")


Alternatively, you can pass a Lua table for the `args` argument and let ngx_lua do URI escaping and string concatenation automatically for you, for instance,


    ngx.exec("/foo", { a = 3, b = "hello world" })


The result is exactly the same as the previous example.

Note that this is very different from [ngx.redirect](http://wiki.nginx.org/HttpLuaModule#ngx.redirect) in that
it's just an internal redirect and no new HTTP traffic is involved.

This method never returns.

This method *must* be called before [ngx.send_headers](http://wiki.nginx.org/HttpLuaModule#ngx.send_headers) or explicit response body
outputs by either [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print) or [ngx.say](http://wiki.nginx.org/HttpLuaModule#ngx.say).

This method is very much like the [echo_exec](http://wiki.nginx.org/HttpEchoModule#echo_exec) directive in [HttpEchoModule](http://wiki.nginx.org/HttpEchoModule).

ngx.redirect
------------
**syntax:** *ngx.redirect(uri, status?)*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Issue an `HTTP 301` or <code>302` redirection to <code>uri`.

The optional `status` parameter specifies whether
`301` or `302` to be used. It's `302` (`ngx.HTTP_MOVED_TEMPORARILY`) by default.

Here's a small example:


    return ngx.redirect("/foo")


which is equivalent to


    return ngx.redirect("http://localhost:1984/foo", ngx.HTTP_MOVED_TEMPORARILY)


assuming the current server name is `localhost` and it's listening on the `1984` port.

This method *must* be called before [ngx.send_headers](http://wiki.nginx.org/HttpLuaModule#ngx.send_headers) or explicit response body outputs by either [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print) or [ngx.say](http://wiki.nginx.org/HttpLuaModule#ngx.say).

This method never returns.

This method is very much like the [rewrite](http://wiki.nginx.org/HttpRewriteModule#rewrite) directive with the `redirect` modifier in the standard
[HttpRewriteModule](http://wiki.nginx.org/HttpRewriteModule), for example, this `nginx.conf` snippet


    rewrite ^ /foo redirect;  # nginx config


is equivalent to the following Lua code


    return ngx.redirect('/foo');  -- lua code


while


    rewrite ^ /foo permanent;  # nginx config


is equivalent to


    return ngx.redirect('/foo', ngx.HTTP_MOVED_PERMANENTLY)  -- Lua code


ngx.send_headers
----------------
**syntax:** *ngx.send_headers()*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Explicitly send out the response headers.

Usually you don't have to send headers yourself. `ngx_lua` will automatically send out headers right before you
output contents via [ngx.say](http://wiki.nginx.org/HttpLuaModule#ngx.say) or [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print).

Headers will also be sent automatically when [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) exits normally.

ngx.headers_sent
----------------
**syntax:** *value = ngx.headers_sent*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns `true` if the response headers have been sent (by ngx_lua), and `false` otherwise.

This API was first introduced in ngx_lua v0.3.1rc6.

ngx.print
---------
**syntax:** *ngx.print(...)*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Emit arguments concatenated to the HTTP client (as response body). If response headers have not been sent yet, this function will first send the headers out, and then output the body data.

Lua `nil` value will result in outputing `"nil"`, and Lua boolean values will emit literal `"true"` or `"false"`, accordingly.

Also, nested arrays of strings are also allowed. The elements in the arrays will be sent one by one. For example


    local table = {
        "hello, ",
        {"world: ", true, " or ", false,
            {": ", nil}}
    }
    ngx.print(table)


will yield the output


    hello, world: true or false: nil


Non-array table arguments will cause a Lua exception to be thrown.

ngx.say
-------
**syntax:** *ngx.say(...)*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Just as [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print) but also emit a trailing newline.

ngx.log
-------
**syntax:** *ngx.log(log_level, ...)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Log arguments concatenated to error.log with the given logging level.

Lua `nil` arguments are accepted and result in literal `"nil"`, and Lua booleans result in literal `"true"` or `"false"` outputs.

The `log_level` argument can take constants like `ngx.ERR` and `ngx.WARN`. Check out [Nginx log level constants](http://wiki.nginx.org/HttpLuaModule#Nginx_log_level_constants) for details.

ngx.flush
---------
**syntax:** *ngx.flush()*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Force flushing the response outputs. This operation has no effect in HTTP 1.0 buffering output mode. See [HTTP 1.0 support](http://wiki.nginx.org/HttpLuaModule#HTTP_1.0_support).

ngx.exit
--------
**syntax:** *ngx.exit(status)*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

When `status >= 200` (i.e., `ngx.HTTP_OK` and above), it will interrupt the execution of the current request and return status code to nginx.

When `status == 0` (i.e., `ngx.OK`), it will only quit the current phase handler (or the content handler if the [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) directive is used) and continue to run laster phases (if any) for the current request.

The `status` argument can be `ngx.OK`, `ngx.ERROR`, `ngx.HTTP_NOT_FOUND`,
`ngx.HTTP_MOVED_TEMPORARILY`, or other [HTTP status constants](http://wiki.nginx.org/HttpLuaModule#HTTP_status_constants).

To return an error page with custom contents, use code snippets like this:


    ngx.status = ngx.HTTP_GONE
    ngx.say("This is our own content")
    -- to cause quit the whole request rather than the current phase handler
    ngx.exit(ngx.HTTP_OK)


The effect in action:


    $ curl -i http://localhost/test
    HTTP/1.1 410 Gone
    Server: nginx/1.0.6
    Date: Thu, 15 Sep 2011 00:51:48 GMT
    Content-Type: text/plain
    Transfer-Encoding: chunked
    Connection: keep-alive

    This is our own content


ngx.eof
-------
**syntax:** *ngx.eof()*

**context:** *rewrite_by_lua*, access_by_lua*, content_by_lua**

Explicitly specify the end of the response output stream.

ngx.escape_uri
--------------
**syntax:** *newstr = ngx.escape_uri(str)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Escape `str` as a URI component.

ngx.unescape_uri
----------------
**syntax:** *newstr = ngx.unescape_uri(str)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Unescape `str` as an escaped URI component.

ngx.encode_base64
-----------------
**syntax:** *newstr = ngx.encode_base64(str)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Encode `str` to a base64 digest.

ngx.decode_base64
-----------------
**syntax:** *newstr = ngx.decode_base64(str)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Decodes the `str` argument as a base64 digest to the raw form. Returns `nil` if `str` is not well formed.

ngx.crc32_short
---------------
**syntax:** *intval = ngx.crc32_short(str)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Calculates the CRC-32 (Cyclic Redundancy Code) digest for the `str` argument.

This method performs better on relatively short `str` inputs (i.e., less than 30 ~ 60 bytes), as compared to [ngx.crc32_long](http://wiki.nginx.org/HttpLuaModule#ngx.crc32_long). The result is exactly the same as [ngx.crc32_long](http://wiki.nginx.org/HttpLuaModule#ngx.crc32_long).

Behind the scene, it is just a thin wrapper around the `ngx_crc32_short` function defined in the Nginx core.

This API was first introduced in the `v0.3.1rc8` release.

ngx.crc32_long
--------------
**syntax:** *intval = ngx.crc32_long(str)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Calculates the CRC-32 (Cyclic Redundancy Code) digest for the `str` argument.

This method performs better on relatively long `str` inputs (i.e., longer than 30 ~ 60 bytes), as compared to [ngx.crc32_short](http://wiki.nginx.org/HttpLuaModule#ngx.crc32_short).  The result is exactly the same as [ngx.crc32_short](http://wiki.nginx.org/HttpLuaModule#ngx.crc32_short).

Behind the scene, it is just a thin wrapper around the `ngx_crc32_long` function defined in the Nginx core.

This API was first introduced in the `v0.3.1rc8` release.

ngx.today
---------
**syntax:** *str = ngx.today()*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns today's date (in the format `yyyy-mm-dd`) from nginx cached time (no syscall involved unlike Lua's date library).

This is the local time.

ngx.time
--------
**syntax:** *secs = ngx.time()*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns the elapsed seconds from the epoch for the current timestamp from the nginx cached time (no syscall involved unlike Lua's date library).

ngx.localtime
-------------
**syntax:** *str = ngx.localtime()*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns the current timestamp (in the format `yyyy-mm-dd hh:mm:ss`) of the nginx cached time (no syscall involved unlike Lua's [os.date](http://www.lua.org/manual/5.1/manual.html#pdf-os.date) function).

This is the local time.

ngx.utctime
-----------
**syntax:** *str = ngx.utctime()*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns the current timestamp (in the format `yyyy-mm-dd hh:mm:ss`) of the nginx cached time (no syscall involved unlike Lua's [os.date](http://www.lua.org/manual/5.1/manual.html#pdf-os.date) function).

This is the UTC time.

ngx.cookie_time
---------------
**syntax:** *str = ngx.cookie_time(sec)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns a formated string can be used as the cookie expiration time. The parameter `sec` is the timestamp in seconds (like those returned from [ngx.time](http://wiki.nginx.org/HttpLuaModule#ngx.time)).

    ngx.say(ngx.cookie_time(1290079655))
        -- yields "Thu, 18-Nov-10 11:27:35 GMT"

ngx.http_time
-------------
**syntax:** *str = ngx.http_time(sec)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns a formated string can be used as the http header time (for example, being used in `Last-Modified` header). The parameter `sec` is the timestamp in seconds (like those returned from [ngx.time](http://wiki.nginx.org/HttpLuaModule#ngx.time)).

    ngx.say(ngx.http_time(1290079655))
        -- yields "Thu, 18 Nov 10 11:27:35 GMT"

ngx.parse_http_time
-------------------
**syntax:** *sec = ngx.parse_http_time(str)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Parse the http time string (as returned by [ngx.http_time](http://wiki.nginx.org/HttpLuaModule#ngx.http_time)) into seconds. Returns the seconds or `nil` if the input string is in bad forms.

    local time = ngx.parse_http_time("Thu, 18 Nov 10 11:27:35 GMT")
    if time == nil then
        ...
    end

ngx.is_subrequest
-----------------
**syntax:** *value = ngx.is_subrequest*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Returns `true` if the current request is an nginx subrequest, or `false` otherwise.

ngx.re.match
------------
**syntax:** *captures = ngx.re.match(subject, regex, options?, ctx?)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Matches the `subject` string using the Perl-compatible regular expression `regex` with the optional `options`.

Only the first occurrence of the match is returned, or `nil` if no match is found. In case of fatal errors, like seeing bad `UTF-8` sequences in `UTF-8` mode, a Lua exception will be raised.

When a match is found, a Lua table `captures` is returned, where `captures[0]` holds the whole substring being matched, and `captures[1]` holds the first parenthesized subpattern's capturing, `captures[2]` the second, and so on. Here's some examples:


    local m = ngx.re.match("hello, 1234", "[0-9]+")
    -- m[0] == "1234"



    local m = ngx.re.match("hello, 1234", "([0-9])[0-9]+")
    -- m[0] == "1234"
    -- m[1] == "1"


Unmatched subpatterns will take `nil` values in their `captures` table fields. For instance,


    local m = ngx.re.match("hello, world", "(world)|(hello)")
    -- m[0] == "hello"
    -- m[1] == nil
    -- m[2] == "hello"


Escaping sequences in Perl-compatible regular expressions like `\d`, `\s`, and `\w`, require special care when specifying them in Lua string literals, because the backslash character, `\`, needs to be escaped in Lua string literals too, for example,


    ? m = ngx.re.match("hello, 1234", "\d+")


won't work as expected and won't match at all. Intead, you should escape the backslash itself and write


    m = ngx.re.match("hello, 1234", "\\d+")


When you put the Lua code snippet in your `nginx.conf` file, you have to escape the backslash one more time, because your Lua code is now in an nginx string literal, and backslashes in nginx string literals require escaping as well. For instance,


    location /test {
        content_by_lua '
            local m = ngx.re.match("hello, 1234", "\\\\d+")
            if m then ngx.say(m[0]) else ngx.say("not matched!") end
        ';
    }


You can also specify `options` to control how the match will be performed. The following option characters are supported:


    a             anchored mode (only match from the beginning)
    i             caseless mode (just like Perl's /i modifier)
    m             multi-line mode (just like Perl's /m modifier)
    o             compile-once mode (similar to Perl's /o modifer),
                  to enable the worker-process-level compiled-regex cache
    s             single-line mode (just like Perl's /s modifier)
    u             UTF-8 mode
    x             extended mode (just like Perl's /x modifier)


These characters can be combined together, for example,


    local m = ngx.re.match("hello, world", "HEL LO", "ix")
    -- m[0] == "hello"



    local m = ngx.re.match("hello, 美好生活", "HELLO, (.{2})", "iu")
    -- m[0] == "hello, 美好"
    -- m[1] == "美好"


The `o` regex option is good for performance tuning, because the regex in question will only be compiled once, cached in the worker-process level, and shared among all the requests in the current Nginx worker process. You can tune the upper limit of the regex cache via the [lua_regex_cache_max_entries](http://wiki.nginx.org/HttpLuaModule#lua_regex_cache_max_entries) directive.

The optional fourth argument, `ctx`, can be a Lua table holding an optional `pos` field. When the `pos` field in the `ctx` table argument is specified, `ngx.re.match` will start matching from that offset. Regardless of the presence of the `pos` field in the `ctx` table, `ngx.re.match` will always set this `pos` field to the position *after* the substring matched by the whole pattern in case of a successful match. When match fails, the `ctx` table will leave intact. Here is some examples,


    local ctx = {}
    local m = ngx.re.match("1234, hello", "[0-9]+", "", ctx)
         -- m[0] = "1234"
         -- ctx.pos == 4



    local ctx = { pos = 2 }
    local m = ngx.re.match("1234, hello", "[0-9]+", "", ctx)
         -- m[0] = "34"
         -- ctx.pos == 4


The `ctx` table argument combined with the `a` regex modifier can be used to construct a lexer atop `ngx.re.match`.

Note that, the `options` argument is not optional when the `ctx` argument is specified; use the empty Lua string (`""`) as the placeholder for `options` if you do not want to specify any regex options.

This method requires the PCRE library enabled in your Nginx build.

This feature is introduced in the `v0.2.1rc11` release.

ngx.re.gmatch
-------------
**syntax:** *iterator = ngx.re.gmatch(subject, regex, options?)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Similar to [ngx.re.match](http://wiki.nginx.org/HttpLuaModule#ngx.re.match), but returns a Lua iterator instead, so as to let the user programmer iterate all the matches over the `<subject>` string argument with the Perl-compatible regular expression `regex`.

Here's a small exmple to demonstrate its basic usage:


    local iterator = ngx.re.gmatch("hello, world!", "([a-z]+)", "i")
    local m
    m = iterator()    -- m[0] == m[1] == "hello"
    m = iterator()    -- m[0] == m[1] == "world"
    m = iterator()    -- m == nil


More often we just put it into a Lua `for` loop:


    for m in ngx.re.gmatch("hello, world!", "([a-z]+)", "i")
        ngx.say(m[0])
        ngx.say(m[1])
    end


The optional `options` argument takes exactly the same semantics as the [ngx.re.match](http://wiki.nginx.org/HttpLuaModule#ngx.re.match) method.

The current implementation requires that the iterator returned should only be used in a single request. That is, one should *not* assign it to a variable belonging to persistent namespace like a Lua package.

This method requires the PCRE library enabled in your Nginx build.

This feature was first introduced in the `v0.2.1rc12` release.

ngx.re.sub
----------
**syntax:** *newstr, n = ngx.re.sub(subject, regex, replace, options?)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Substitutes the first match of the Perl-compatible regular expression `regex` on the `subject` argument string with the string or function argument `replace`. The optional `options` argument has exactly the same meaning as in [ngx.re.match](http://wiki.nginx.org/HttpLuaModule#ngx.re.match).

This method returns the resulting new string as well as the number of successful substitutions, or throw out a Lua exception when an error occurred (syntax errors in the `<replace>` string argument, for example).

When the `replace` is a string, then it is treated as a special template for string replacement. For example,


    local newstr, n = ngx.re.sub("hello, 1234", "([0-9])[0-9]", "[$0][$1]")
        -- newstr == "hello, [12][1]34"
        -- n == 1


where `$0` referring to the whole substring matched by the pattern and `$1` referring to the first parenthesized capturing substring.

You can also use curly braces to disambiguate variable names from the background string literals: 


    local newstr, n = ngx.re.sub("hello, 1234", "[0-9]", "${0}00")
        -- newstr == "hello, 10034"
        -- n == 1


Literal dollar sign characters (`$`) in the `replace` string argument can be escaped by another dollar sign, for instance,


    local newstr, n = ngx.re.sub("hello, 1234", "[0-9]", "$$")
        -- newstr == "hello, $234"
        -- n == 1


Do not use backlashes to escape dollar signs; it won't work as expected.

When the `replace` argument is of type "function", then it will be invoked with the "match table" as the argument to generate the replace string literal for substitution. The "match table" fed into the `replace` function is exactly the same as the return value of [ngx.re.match](http://wiki.nginx.org/HttpLuaModule#ngx.re.match). Here is an example:


    local func = function (m)
        return "[" .. m[0] .. "][" .. m[1] .. "]"
    end
    local newstr, n = ngx.re.sub("hello, 1234", "( [0-9] ) [0-9]", func, "x")
        -- newstr == "hello, [12][1]34"
        -- n == 1


The dollar sign characters in the return value of the `replace` function argument are not special at all.

This method requires the PCRE library enabled in your Nginx build.

This feature was first introduced in the `v0.2.1rc13` release.

ngx.re.gsub
-----------
**syntax:** *newstr, n = ngx.re.gsub(subject, regex, replace, options?)*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

Just like [ngx.re.sub](http://wiki.nginx.org/HttpLuaModule#ngx.re.sub), but does global substitution.

Here is some examples:


    local newstr, n = ngx.re.gsub("hello, world", "([a-z])[a-z]+", "[$0,$1]", "i")
        -- newstr == "[hello,h], [world,w]"
        -- n == 2



    local func = function (m)
        return "[" .. m[0] .. "," .. m[1] .. "]"
    end
    local newstr, n = ngx.re.gsub("hello, world", "([a-z])[a-z]+", func, "i")
        -- newstr == "[hello,h], [world,w]"
        -- n == 2


This method requires the PCRE library enabled in your Nginx build.

This feature was first introduced in the `v0.2.1rc15` release.

ndk.set_var.DIRECTIVE
---------------------
**syntax:** *res = ndk.set_var.DIRECTIVE_NAME*

**context:** *set_by_lua*, rewrite_by_lua*, access_by_lua*, content_by_lua*, header_filter_by_lua**

This mechanism allows calling other nginx C modules' directives that are implemented by [Nginx Devel Kit](https://github.com/simpl/ngx_devel_kit) (NDK)'s set_var submodule's `ndk_set_var_value`.

For example, [HttpSetMiscModule](http://wiki.nginx.org/HttpSetMiscModule)'s following directives can be invoked this way:

* [set_quote_sql_str](http://wiki.nginx.org/HttpSetMiscModule#set_quote_sql_str)
* [set_quote_pgsql_str](http://wiki.nginx.org/HttpSetMiscModule#set_quote_pgsql_str)
* [set_quote_json_str](http://wiki.nginx.org/HttpSetMiscModule#set_quote_json_str)
* [set_unescape_uri](http://wiki.nginx.org/HttpSetMiscModule#set_unescape_uri)
* [set_escape_uri](http://wiki.nginx.org/HttpSetMiscModule#set_escape_uri)
* [set_encode_base32](http://wiki.nginx.org/HttpSetMiscModule#set_encode_base32)
* [set_decode_base32](http://wiki.nginx.org/HttpSetMiscModule#set_decode_base32)
* [set_encode_base64](http://wiki.nginx.org/HttpSetMiscModule#set_encode_base64)
* [set_decode_base64](http://wiki.nginx.org/HttpSetMiscModule#set_decode_base64)
* [set_encode_hex](http://wiki.nginx.org/HttpSetMiscModule#set_encode_base64)
* [set_decode_hex](http://wiki.nginx.org/HttpSetMiscModule#set_decode_base64)
* [set_sha1](http://wiki.nginx.org/HttpSetMiscModule#set_encode_base64)
* [set_md5](http://wiki.nginx.org/HttpSetMiscModule#set_decode_base64)

For instance,


    local res = ndk.set_var.set_escape_uri('a/b');
    -- now res == 'a%2fb'


Similarly, the following directives provided by [HttpEncryptedSessionModule](http://wiki.nginx.org/HttpEncryptedSessionModule) can be invoked from within Lua too:

* [set_encrypt_session](http://wiki.nginx.org/HttpEncryptedSessionModule#set_encrypt_session)
* [set_decrypt_session](http://wiki.nginx.org/HttpEncryptedSessionModule#set_decrypt_session)

This feature requires the [ngx_devel_kit](https://github.com/simpl/ngx_devel_kit) module.

HTTP 1.0 support
================

The HTTP 1.0 protocol does not support chunked outputs and always requires an
explicit `Content-Length` header when the response body is non-empty. So when
an HTTP 1.0 request is present, This module will automatically buffer all the
outputs of user calls of [ngx.say](http://wiki.nginx.org/HttpLuaModule#ngx.say) and [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print) and
postpone sending response headers until it sees all the outputs in the response
body, and at that time ngx_lua can calculate the total length of the body and
construct a proper `Content-Length` header for the HTTP 1.0 client.

Note that, common HTTP benchmark tools like `ab` and `http_load` always issue
HTTP 1.0 requests by default. To force `curl` to send HTTP 1.0 requests, use
the `-0` option.

Data Sharing within an Nginx Worker
===================================

**NOTE: This mechanism behaves differently when code cache is turned off, and should be considered as a DIRTY TRICK. Backward compatibility is NOT guaranteed. Use at your own risk! We're going to design a whole new data-sharing mechanism.**

If you want to globally share user data among all the requests handled by the same nginx worker process, you can encapsulate your shared data into a Lua module, require the module in your code, and manipulate shared data through it. It works because required Lua modules are loaded only once, and all coroutines will share the same copy of the module.

Here's a complete small example:


    -- mydata.lua
    module("mydata", package.seeall)
 
    local data = {
        dog = 3,
        cat = 4,
        pig = 5,
    }
 
    function get_age(name)
        return data[name]
    end


and then accessing it from your nginx.conf:


    location /lua {
        content_lua_by_lua '
            local mydata = require("mydata")
            ngx.say(mydata.get_age("dog"))
        ';
    }


Your `mydata` module in this example will only be loaded
and run on the first request to the location `/lua`,
and all those subsequent requests to the same nginx
worker process will use the reloaded instance of the
module as well as the same copy of the data in it,
until you send a `HUP` signal to the nginx master
process to enforce a reload.

This data sharing technique is essential for high-performance Lua apps built atop this module. It's common to cache reusable data globally.

It's worth noting that this is *per-worker* sharing, not *per-server* sharing. That is, when you have multiple nginx worker processes under an nginx master, this data sharing cannot pass process boundary. If you indeed need server-wide data sharing, you can

1. Use only a single nginx worker and a single server. This is not recommended when you have a multi-core CPU or multiple CPUs in a single machine.
1. Use some true backend storage like `memcached`, `redis`, or an RDBMS like `mysql`.

Performance
===========

The Lua state (aka the Lua vm instance) is shared across all the requests
handled by a single nginx worker process to miminize memory use.

On a ThinkPad T400 2.80 GHz laptop, it's easy to achieve 25k req/sec using ab
w/o keepalive and 37k+ req/sec with keepalive.

You can get better performance when building this module
with LuaJIT 2.0.

Installation
============

You're recommended to install this module as well as the Lua interpreter or LuaJIT 2.0 (with many other good stuffs) via the ngx_openresty bundle:

<http://openresty.org> 

The installation steps are usually as simple as `./configure && make && make install`.

Alternatively, you can compile this module with nginx core's source by hand:

1. Install Lua or LuaJIT into your system. At least Lua 5.1 is required.  Lua can be obtained freely from its project [homepage](http://www.lua.org/).  For Ubuntu/Debian users, just install the liblua5.1-0-dev package (or something like that).
1. Download the latest version of the release tarball of the ngx_devel_kit (NDK) module from lua-nginx-module [file list](http://github.com/simpl/ngx_devel_kit/downloads).
1. Download the latest version of the release tarball of this module from lua-nginx-module [file list](http://github.com/chaoslawful/lua-nginx-module/downloads).
1. Grab the nginx source code from [nginx.org](http://nginx.org/), for example, the version 1.0.5 (see nginx compatibility), and then build the source with this module:

        $ wget 'http://nginx.org/download/nginx-1.0.5.tar.gz'
        $ tar -xzvf nginx-1.0.5.tar.gz
        $ cd nginx-1.0.5/
 
        # tell nginx's build system where to find lua:
        export LUA_LIB=/path/to/lua/lib
        export LUA_INC=/path/to/lua/include
 
        # or tell where to find LuaJIT when you want to use JIT instead
        # export LUAJIT_LIB=/path/to/luajit/lib
        # export LUAJIT_INC=/path/to/luajit/include/luajit-2.0
 
        # Here we assume you would install you nginx under /opt/nginx/.
        $ ./configure --prefix=/opt/nginx \
            --add-module=/path/to/ngx_devel_kit \
            --add-module=/path/to/lua-nginx-module
 
        $ make -j2
        $ make install


Compatibility
=============

The following versions of Nginx should work with this module:

*   1.0.x (last tested: 1.0.6)
*   0.9.x (last tested: 0.9.4)
*   0.8.x >= 0.8.54 (last tested: 0.8.54)

Earlier versions of Nginx like 0.6.x and 0.5.x will **not** work.

If you find that any particular version of Nginx above 0.8.54 does not
work with this module, please consider reporting a bug.

Report Bugs
===========

Although a lot of effort has been put into testing and code tuning, there must be some serious bugs lurking somewhere in this module. So whenever you are bitten by any quirks, please don't hesitate to

1. create a ticket on the [issue tracking interface](http://github.com/chaoslawful/lua-nginx-module/issues) provided by GitHub,
1. or send a bug report or even patches to the [nginx mailing list](http://mailman.nginx.org/mailman/listinfo/nginx).

Source Repository
=================

Available on github at [chaoslawful/lua-nginx-module](http://github.com/chaoslawful/lua-nginx-module).

Test Suite
==========

To run the test suite, you also need the following dependencies:

* Nginx version >= 0.8.54

* Perl modules:
	* test-nginx: <http://github.com/agentzh/test-nginx> 

* Nginx modules:
	* echo-nginx-module: <http://github.com/agentzh/echo-nginx-module> 
	* drizzle-nginx-module: <http://github.com/chaoslawful/drizzle-nginx-module> 
	* rds-json-nginx-module: <http://github.com/agentzh/rds-json-nginx-module> 
	* set-misc-nginx-module: <http://github.com/agentzh/set-misc-nginx-module> 
	* headers-more-nginx-module: <http://github.com/agentzh/headers-more-nginx-module> 
	* memc-nginx-module: <http://github.com/agentzh/memc-nginx-module> 
	* srcache-nginx-module: <http://github.com/agentzh/srcache-nginx-module> 
	* ngx_auth_request: <http://mdounin.ru/hg/ngx_http_auth_request_module/> 

* C libraries:
	* yajl: <https://github.com/lloyd/yajl> 

* Lua modules:
	* lua-yajl: <https://github.com/brimworks/lua-yajl> 
		* Note: the compiled module has to be placed in '/usr/local/lib/lua/5.1/'

* Applications:
	* mysql: create database 'ngx_test', grant all privileges to user 'ngx_test', password is 'ngx_test'
	* memcached

These module's adding order is IMPORTANT! For filter modules's position in
filtering chain affects a lot. The correct configure adding order is:

1. ngx_devel_kit
1. set-misc-nginx-module
1. ngx_http_auth_request_module
1. echo-nginx-module
1. memc-nginx-module
1. lua-nginx-module (i.e. this module)
1. headers-more-nginx-module
1. srcache-nginx-module
1. drizzle-nginx-module
1. rds-json-nginx-module

TODO
====

* Add `ignore_resp_headers`, `ignore_resp_body`, and `ignore_resp` options to [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) and ngx.location.capture_multi` methods, to allow micro performance tuning on the user side.
* Add directives to run lua codes when nginx stops/reloads.
* Deal with TCP 3-second delay problem under great connection harness.

Future Plan
===========

* Add the `lua_require` directive to load module into main thread's globals.
* Add the "cosocket" mechamism that will emulate a common set of Lua socket API that will give you totally transparently non-blocking capability out of the box by means of a completely new upstream layer atop the nginx event model and no nginx subrequest overheads.
* Add Lua code automatic time slicing support by yielding and resuming the Lua VM actively via Lua's debug hooks.
* Make set_by_lua using the same mechanism as content_by_lua.

Known Issues
============

* As ngx_lua's predefined Nginx I/O APIs use coroutine yielding/resuming mechanism, the user code should not call any Lua modules that use coroutine API to prevent obfuscating the predefined Nginx APIs like [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) (actually coroutine modules have been masked off in [content_by_lua](http://wiki.nginx.org/HttpLuaModule#content_by_lua) directives and others). This limitation is a little crucial, but don't worry, we're working on an alternative coroutine implementation that can fit into the Nginx event model. When it is done, the user code will be able to use the Lua coroutine mechanism freely as in standard Lua again!
* Because the standard Lua 5.1 interpreter's VM is not fully resumable, the methods [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture), [[#ngx.location.capture_multi|ngx.location.capture_multi], [ngx.redirect](http://wiki.nginx.org/HttpLuaModule#ngx.redirect), [ngx.exec](http://wiki.nginx.org/HttpLuaModule#ngx.exec), and [ngx.exit](http://wiki.nginx.org/HttpLuaModule#ngx.exit) cannot be used within the context of a Lua [pcall()](http://www.lua.org/manual/5.1/manual.html#pdf-pcall) or [xpcall()](http://www.lua.org/manual/5.1/manual.html#pdf-xpcall) when the standard Lua 5.1 interpreter is used; you'll get the error `attempt to yield across metamethod/C-call boundary`. To fix this, please use LuaJIT 2.0 instead, because LuaJIT 2.0 supports a fully resume-able VM.
* The [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) and [ngx.location.capture_multi](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture_multi) Lua methods cannot capture locations configured by [HttpEchoModule](http://wiki.nginx.org/HttpEchoModule)'s [echo_location](http://wiki.nginx.org/HttpEchoModule#echo_location), [echo_location_async](http://wiki.nginx.org/HttpEchoModule#echo_location_async), [echo_subrequest](http://wiki.nginx.org/HttpEchoModule#echo_subrequest), or [echo_subrequest_async](http://wiki.nginx.org/HttpEchoModule#echo_subrequest_async) directives. This won't be fixed in the future due to technical problems.
* **WATCH OUT: Globals WON'T persist between requests**, because of the one-coroutine-per-request isolation design. Especially watch yourself when using `require()` to import modules, and use this form:

        local xxx = require('xxx')

	instead of the old deprecated form:

        require('xxx')

	The old form will cause module unusable in requests for the reason told previously. If you have to stick with the old form, you can always force loading module for every request by clean `package.loaded.<module>`, like this:

        package.loaded.xxx = nil
        require('xxx')

* It's recommended to always put the following piece of code at the end of your Lua modules using [ngx.location.capture](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture) or [ngx.location.capture_multi](http://wiki.nginx.org/HttpLuaModule#ngx.location.capture_multi) to prevent casual use of module-level global variables that are shared among *all* requests, which is usually not what you want:

    getmetatable(foo.bar).__newindex = function (table, key, val)
        error('Attempt to write to undeclared variable "' .. key .. '": '
                .. debug.traceback())
    end

assuming your current Lua module is named `foo.bar`. This will guarantee that you have declared your Lua functions' local Lua variables as "local" in your Lua modules, or bad race conditions while accessing these variables under load will tragically happen. See the `Data Sharing within an Nginx Worker` for the reasons of this danger.

Changes
=======

v0.3.0
------
**New features**

* added the [header_filter_by_lua](http://wiki.nginx.org/HttpLuaModule#header_filter_by_lua) and [header_filter_by_lua_file](http://wiki.nginx.org/HttpLuaModule#header_filter_by_lua_file) directives. thanks Liseen Wan (万珣新).
* implemented the PCRE regex API for Lua: [ngx.re.match](http://wiki.nginx.org/HttpLuaModule#ngx.re.match), [ngx.re.gmatch](http://wiki.nginx.org/HttpLuaModule#ngx.re.gmatch), [ngx.re.sub](http://wiki.nginx.org/HttpLuaModule#ngx.re.sub), and [ngx.re.gsub](http://wiki.nginx.org/HttpLuaModule#ngx.re.gsub).
* now we add the `ngx` and `ndk` table into `package.loaded` such that the user can write `local ngx = require 'ngx'` and `local ndk = require 'ndk'`. thanks @Lance.
* added new directive [lua_regex_cache_max_entries](http://wiki.nginx.org/HttpLuaModule#lua_regex_cache_max_entries) to control the upper limit of the worker-process-level compiled-regex cache enabled by the `o` regex option.
* implemented the special [ngx.ctx](http://wiki.nginx.org/HttpLuaModule#ngx.ctx) Lua table for user programmers to store per-request Lua context data for their applications. thanks 欧远宁 for suggesting this feature.
* now [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print) and [ngx.say](http://wiki.nginx.org/HttpLuaModule#ngx.say) allow (nested) array-like table arguments. the array elements in them will be sent piece by piece. this will avoid string concatenation for templating engines like [ltp](http://www.savarese.com/software/ltp/).
* implemented the [ngx.req.get_post_args](http://wiki.nginx.org/HttpLuaModule#ngx.req.get_post_args) method for fetching url-encoded POST query arguments from within Lua.
* implemented the [ngx.req.get_uri_args](http://wiki.nginx.org/HttpLuaModule#ngx.req.get_uri_args) method to fetch parsed URL query arguments from within Lua. thanks Bertrand Mansion (golgote).
* added new function [ngx.parse_http_time](http://wiki.nginx.org/HttpLuaModule#ngx.parse_http_time), thanks James Hurst.
* now we allow Lua boolean and `nil` values in arguments to [ngx.say](http://wiki.nginx.org/HttpLuaModule#ngx.say), [ngx.print](http://wiki.nginx.org/HttpLuaModule#ngx.print), [ngx.log](http://wiki.nginx.org/HttpLuaModule#ngx.log) and [print](http://wiki.nginx.org/HttpLuaModule#print).
* added support for user C macros `LUA_DEFAULT_PATH` and `LUA_DEFAULT_CPATH`. for now we can only define them in `ngx_lua`'s `config` file because nginx `configure`'s `--with-cc-opt` option hates values with double-quotes in them. sigh. [ngx_openresty](http://openresty.org/) is already using this feature to bundle 3rd-party Lua libraries.

**Bug fixes**

* worked-around the "stack overflow" issue while using `luarocks.loader` and disabling [lua_code_cache](http://wiki.nginx.org/HttpLuaModule#lua_code_cache), as described as github issue #27. thanks Patrick Crosby.
* fixed the `zero size buf in output` alert while combining [lua_need_request_body](http://wiki.nginx.org/HttpLuaModule#lua_need_request_body) on + [access_by_lua](http://wiki.nginx.org/HttpLuaModule#access_by_lua)/[rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) + [proxy_pass](http://wiki.nginx.org/HttpProxyModule#proxy_pass)/[fastcgi_pass](http://wiki.nginx.org/HttpFcgiModule#fastcgi_pass). thanks Liseen Wan (万珣新).
* fixed issues with HTTP 1.0 HEAD requests.
* made setting `ngx.header.HEADER` after sending out response headers throw out a Lua exception to help debugging issues like github issue #49. thanks Bill Donahue (ikhoyo).
* fixed an issue regarding defining global variables in C header files: we should have defined the global `ngx_http_lua_exception` in a single compilation unit. thanks @姜大炮.

Authors
=======

* chaoslawful (王晓哲) <chaoslawful at gmail dot com>
* Zhang "agentzh" Yichun (章亦春) <agentzh at gmail dot com>

Copyright & License
===================

This module is licenced under the BSD license.

Copyright (C) 2009, 2010, 2011, Taobao Inc., Alibaba Group ( <http://www.taobao.com> ).

Copyright (C) 2009, 2010, 2011, by Xiaozhe Wang (chaoslawful) <chaoslawful@gmail.com>.

Copyright (C) 2009, 2010, 2011, by Zhang "agentzh" Yichun (章亦春) <agentzh@gmail.com>.

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

See Also
========

* [Dynamic Routing Based on Redis and Lua](http://openresty.org/#DynamicRoutingBasedOnRedis)
* [Using LuaRocks with ngx_lua](http://openresty.org/#UsingLuaRocks)
* [Introduction to ngx_lua](https://github.com/chaoslawful/lua-nginx-module/wiki/Introduction)
* [ngx_devel_kit](http://github.com/simpl/ngx_devel_kit)
* [HttpEchoModule](http://wiki.nginx.org/HttpEchoModule)
* [HttpDrizzleModule](http://wiki.nginx.org/HttpDrizzleModule)
* [postgres-nginx-module](http://github.com/FRiCKLE/ngx_postgres)
* [HttpMemcModule](http://wiki.nginx.org/HttpMemcModule)
* [The ngx_openresty bundle](http://openresty.org)

