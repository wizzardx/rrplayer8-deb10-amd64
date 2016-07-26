
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

#include "fake_xmmsctrl.h"

#include "testing.h"

// david@david:/tmp/x/xmlrpc-c-1.33.06/src$ kwrite xmlrpc_build.c
// - function - getValue - for the misc format strings

gboolean fake_xmms_remote_is_playing(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_is_playing");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const is_playing((xmlrpc_c::value_boolean(result)));

    return is_playing;
}

gboolean fake_xmms_remote_is_paused(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_is_paused");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const is_paused((xmlrpc_c::value_boolean(result)));

    return is_paused;
}

gboolean fake_xmms_remote_is_running(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_is_running");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const is_running((xmlrpc_c::value_boolean(result)));

    return is_running;
}

void fake_xmms_remote_stop(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_stop");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);
}

void fake_xmms_remote_playlist_clear(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_playlist_clear");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);
}

void fake_xmms_remote_play(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_play");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);
}

void fake_xmms_remote_playlist_add_url_string(gint session, gchar * url) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_playlist_add_url_string");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "is", &result, session, url);
}

void fake_xmms_remote_set_main_volume(gint session, gint v) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_set_main_volume");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "ii", &result, session, v);
}

gboolean fake_xmms_remote_is_repeat(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_is_repeat");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const is_repeat((xmlrpc_c::value_boolean(result)));

    return is_repeat;
}

gint fake_xmms_remote_get_main_volume(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_get_main_volume");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const main_volume((xmlrpc_c::value_int(result)));

    return main_volume;
}

gint fake_xmms_remote_get_playlist_length(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_get_playlist_length");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const playlist_length((xmlrpc_c::value_int(result)));

    return playlist_length;
}

gint fake_xmms_remote_get_output_time(gint session) {
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_get_output_time");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const output_time((xmlrpc_c::value_int(result)));

    return output_time;
}

gint fake_xmms_remote_get_current_song_length_ms(gint session) {
    // Haha my fake routine...
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_get_current_song_length_ms");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const song_length_ms((xmlrpc_c::value_int(result)));

    return song_length_ms;
}

string fake_xmms_remote_get_current_song_title(gint session) {
    // Haha my fake routine...
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_get_current_song_title");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const song_title((xmlrpc_c::value_string(result)));

    return song_title;
}

string fake_xmms_remote_get_current_song_path(gint session) {
    // Haha my fake routine...
    string const serverUrl("http://localhost:30126/RPC2");
    string const methodName("fake_xmms_remote_get_current_song_path");

    xmlrpc_c::clientSimple myClient;
    xmlrpc_c::value result;

    myClient.call(serverUrl, methodName, "i", &result, session);

    auto const song_title((xmlrpc_c::value_string(result)));

    return song_title;
}

void fake_xmms_remote_toggle_repeat(gint session) {
    undefined_throw;
}

